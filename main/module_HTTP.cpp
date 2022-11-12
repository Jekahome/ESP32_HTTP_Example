#include "module_HTTP.h"
#include "hex_html_page.h"

//--------------------------------------------------------------------------------------------------------------------
/*
Способ передать картинку(png,favicon,...) в html
1. Конвертация image в base64 https://www.base64-image.de/ и <img src="data:image/png;base64,iVBOR..."">
2. Включить в сбоку двоичный файл idf_component_register(... EMBED_FILES "winter.png" ...) и использовать asm(...)

Способ передать html разметку    
1. Формат текста  const char index_html[] = R"rawliteral(<!DOCTYPE HTML><html><head>...)rawliteral"; httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
2. Фoрмат base64 
   Конвертировать из строки в `HEX` https://www.rapidtables.com/convert/number/ascii-to-hex.html с разделителем `,0x`
   const unsigned char hex_html[] = {0x3C,0x21,0x44.....};
   httpd_resp_send(req,  (char*)hex_html, HTTPD_RESP_USE_STRLEN);

Конвертировать из строки в `HEX` https://www.rapidtables.com/convert/number/ascii-to-hex.html с разделителем `,0x`
Использовать формат HEX система более компактней чем строку. HEX это 16 ричная система и упаковывает набор байтов
так как один символ char это 1 байт т.е. 8 бит помещается 0-255 значений,а в одно значение HEX 2 байта т.е. 16 бит помещается 0-65536 значений
 
*/
//--------------------------------------------------------------------------------------------------------------------
#define GPIO_LED     gpio_num_t::GPIO_NUM_4

static const char *TAG = "Server HTTP";
httpd_handle_t server_httpd = NULL;

module_HTTP::module_HTTP(){
    // конфигурация пина GPIO_LED
    gpio_config_t io_conf = {};
    io_conf.intr_type = gpio_int_type_t::GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (uint64_t)1<<GPIO_LED;// for GPIO_LED pin 4
    io_conf.mode = gpio_mode_t::GPIO_MODE_INPUT_OUTPUT;
    io_conf.pull_up_en = gpio_pullup_t::GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_set_level(GPIO_LED, (uint32_t)0);
}

module_HTTP::~module_HTTP(){
    ESP_LOGW(TAG,"Server GOODBAY"); 
}

size_t size_content(char* buf,size_t max){
    if(buf[0]=='\0'){
        return 0;
    }    
    size_t pos = 0;
    while(--max){
        if(buf[pos]=='\0'){
            return pos;
        }
        pos++;
    }
    return max;
}

//--------------------------------------------------------------------------------------------------------------------

// Client handler
esp_err_t _http_event_handler(esp_http_client_event_t *evt){
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:{
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        }
        case HTTP_EVENT_ON_CONNECTED:{
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        }
        case HTTP_EVENT_HEADER_SENT:{
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        }
        case HTTP_EVENT_ON_HEADER:{
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        }
        case HTTP_EVENT_ON_DATA:{
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                if (evt->user_data) {
                    memcpy((uint8_t*)evt->user_data + output_len, evt->data, evt->data_len);
                } else {
                    if (output_buffer == NULL) {
                        output_buffer = (char *) malloc(esp_http_client_get_content_length(evt->client));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    memcpy(output_buffer + output_len, evt->data, evt->data_len);
                }
                output_len += evt->data_len;
            }

            break;
        }
        case HTTP_EVENT_ON_FINISH:{
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        }
        case HTTP_EVENT_DISCONNECTED:{
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        }
        case HTTP_EVENT_REDIRECT:{
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            break;            
        }

    }
    return ESP_OK;
}

//--------------------------------------------------------------------------------------------------------------------

typedef struct {
    char    *username;
    char    *password;
} basic_auth_info_t;

#define HTTPD_401      "401 UNAUTHORIZED"          

char *module_HTTP::http_auth_basic(const char *username, const char *password){
    int out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    asprintf(&user_info, "%s:%s", username, password);
    if (!user_info) {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = (char*)calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, (size_t *)&out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

// Проверка Аутентификации (подлинность пользователя)
// Аутентифика́ция -  процедура проверки легальности пользователя или данных, например, 
// проверки соответствия введённого пользователем пароля к паролю учётной записи в базе данны

// Не следует путать Аутентификацию с Авторизацией или с Идентификацией
//
// 1.Авторизация - предоставление определённому лицу или группе лиц прав на выполнение определённых действий/полномочий; 
// а также процесс проверки (подтверждения) данных прав при попытке выполнения этих действий. Часто можно услышать выражение, 
// что какой-то человек «авторизован» для выполнения данной операции — это значит, что он имеет на неё право.
//
// 2.Идентификация - для субъекта идентификации выявляется его идентификатор, однозначно идентифицирующий этого субъекта
//
// Получает из заголовка Authorization закодированную base64 сторку `Base логин:пароль`
// и сравнивает с текущим закодированным значением из user_ctx 
bool module_HTTP::check_auth(httpd_req_t *req){
     
    char *buf = NULL;
    size_t buf_len = 0;
    basic_auth_info_t *basic_auth_info = (basic_auth_info_t*)req->user_ctx;
    bool ret = true;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;// Head Authorization => `curl --user Jeka:1234 ....`
    if (buf_len > 1) {
        buf = (char*)calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
             
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = module_HTTP::http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            ret = true;
        }else{
            ret = false;
            ESP_LOGI(TAG,"Authorization successfull");
        }
    } else{
        ESP_LOGE(TAG, "Not found HEAD Authorization\n");
    }
    if (ret){
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        if (req->method == 1){// GET
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
        }else{ // POST
            httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "UNAUTHORIZED");
        }
        
        return ESP_OK;
    }
    return !ret;
}
 
//--------------------------------------------------------------------------------------------------------------------
// Задача: парсить JSON и обрабатывать команды
//
// An HTTP POST handler 
//
// curl --user Jeka:1234 -d '{"command":3,"file":"SENTENCE.RAW"}' -H "Content-Type: application/json" -X POST 192.168.1.166:80/command
// curl --user Jeka:1234 -d '{"command":9}' -H "Content-Type: application/json" -X POST 192.168.1.166:80/command
// curl --user Jeka:1234 -d '{"command":13}' -H "Content-Type: application/json" 192.168.1.166:80/command
// TODO: `--user Jeka:1234` попадает в заголовок в виде `Authorization: Basic SmVrYToxMjM0`
esp_err_t module_HTTP::json_post_handler(httpd_req_t *req){
    if (!module_HTTP::check_auth(req)){ 
        return ESP_OK;
    }

    WORD_ALIGNED_ATTR char content[128];
    WORD_ALIGNED_ATTR size_t recv_size = min(req->content_len, sizeof(content));
    WORD_ALIGNED_ATTR int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0) {  
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {  
              ESP_LOGE(TAG, "HTTPD_SOCK_ERR_TIMEOUT" );
              httpd_resp_send_408(req);
        }   
        return ESP_FAIL;
    }

    WORD_ALIGNED_ATTR cJSON *rootJSON = NULL;
    WORD_ALIGNED_ATTR size_t buffer_length = size_content(content,SIZE_MAX);
    WORD_ALIGNED_ATTR const char *return_parse_end;
    WORD_ALIGNED_ATTR cJSON_bool require_null_terminated = 0;
    rootJSON = cJSON_ParseWithLengthOpts((char *)content,buffer_length,&return_parse_end,require_null_terminated);
    if(cJSON_GetErrorPtr()!=NULL){
        ESP_LOGE(TAG, "Error before: %s\n", cJSON_GetErrorPtr());// в UART не идет ?
         
        cJSON_Delete(rootJSON);
        const char resp[] = "Invalid JSON";
        httpd_resp_send(req, resp, strlen(resp));
        return ESP_ERR_INVALID_ARG;
    } 

    WORD_ALIGNED_ATTR int command = -1;
    WORD_ALIGNED_ATTR const cJSON *_command = NULL;
     _command = cJSON_GetObjectItemCaseSensitive(rootJSON, "command");
    if (cJSON_IsNumber(_command) && (_command->valueint)){  
        command =  _command->valueint;
        //printf("Checking command %d %d\n", _command->valueint,command);
    }else{
         ESP_LOGE(TAG, "Invalid JSON arguments.Not found key 'command'" );
         const char resp[] = "Invalid JSON arguments.Not found key 'command'";
         httpd_resp_send(req, resp, strlen(resp));
         return ESP_ERR_INVALID_ARG;
    }

    switch ( command ) {
        case Command::send_picture:
        {   
            // curl -d '{"command":3,"file":"SENTENCE.RAW"}' -H "Content-Type: application/json" -X POST 192.168.1.166:80/command
           
            Command res = Command::error_message;
            const cJSON * _file_command = cJSON_GetObjectItemCaseSensitive(rootJSON, "file");
           
            if (!cJSON_IsString(_file_command) || !(_file_command->valuestring)){  
                ESP_LOGE(TAG, "Error send sound" );
                const char resp[] = "Error send sound";
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                httpd_resp_send(req, resp, strlen(resp));
                return ESP_FAIL;
            }else{
                const char resp[] = "Ok";
                httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
                httpd_resp_send(req, resp, strlen(resp));
                return ESP_OK;
            }
            break;
        }
        case  Command::get_sentence: 
        {                
            // curl -d '{"command":9}' -H "Content-Type: application/json" -X POST 192.168.1.166:80/command
            cJSON_Delete(rootJSON);
            const char resp[] = "Ok";
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            httpd_resp_send(req, resp, strlen(resp));
            return ESP_OK;
            break; 
        }
        case Command::esp32cam_restart:
        {
            // curl -d '{"command":13}' -H "Content-Type: application/json" 192.168.1.166:80/command
            cJSON_Delete(rootJSON);
            ESP_LOGI(TAG, "Ok\n" );
            const char resp[] = "Ok";
            httpd_resp_send(req, resp, strlen(resp));
            esp_restart();
            return ESP_OK; 
            break;
        }
        default: 
            cJSON_Delete(rootJSON);
                  const char resp[] = "Invalid arguments.\nExample request:\n \
For send picture => curl -d '{\"command\":3,\"file\":\"SENTENCE.RAW\"}' -H \"Content-Type: application/json\" 192.168.1...:80/command\n \
For get sentence => curl -d '{\"command\":9}' -H \"Content-Type: application/json\" 192.168.1...:80/command\n \
For esp32cam restart => curl -d '{\"command\":13}' -H \"Content-Type: application/json\" 192.168.1...:80/command\n \
";
              httpd_resp_send(req, resp, strlen(resp));
              return ESP_ERR_INVALID_ARG;
     }

      const char resp[] = "Ok";
      httpd_resp_send(req, resp, strlen(resp));
      return ESP_OK;
}

//--------------------------------------------------------------------------------------------------------------------
// Задача: отдать данные обратно
//
// An HTTP POST handler 
//
// curl --user Jeka:1234 -d 'Hello World!'  192.168.1.166:80/echo
// TODO: `--user Jeka:1234` попадает в заголовок в виде `Authorization: Basic SmVrYToxMjM0`
esp_err_t module_HTTP::echo_post_handler(httpd_req_t *req){
    // req->method == 3
    if (!module_HTTP::check_auth(req)){ 
        return ESP_OK;
    }
      
    char buf[100];
    int ret, remaining = req->content_len;

    while (remaining > 0) {
        // Read the data for the request 
        if ((ret = httpd_req_recv(req, buf, min(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                // Retry receiving if timeout occurred 
                continue;
            }
            return ESP_FAIL;
        }

        // Send back the same data 
        httpd_resp_send_chunk(req, buf, ret);
        remaining -= ret;

        // Log data received 
        ESP_LOGI(TAG, "=========== RECEIVED DATA ==========");
        ESP_LOGI(TAG, "%.*s", ret, buf);
        ESP_LOGI(TAG, "====================================");
    }

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}
//--------------------------------------------------------------------------------------------------------------------
 
// An HTTP GET handler 
// TODO:`page_hex_html` from file hex_html_page.h
//
// http://192.168.1.166:80/
esp_err_t module_HTTP::index_get_handler(httpd_req_t *req){
    // GET req->method == 1
    //module_HTTP::check_auth(req);
  
    httpd_resp_set_type(req, "text/html");
    //httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    httpd_resp_send(req,  (char*)page_hex_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// An HTTP POST handler 
esp_err_t ledOn(httpd_req_t *req) {
    ESP_LOGI(TAG, "ON LED"); 
    //gpio_set_level(GPIO_LED, !gpio_get_level(GPIO_LED));
    if (gpio_get_level(GPIO_LED) == 0) {gpio_set_level(GPIO_LED, 1);}
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, "application/json");
    char ledJSON[100];
    sprintf(ledJSON, "{\"on_off_led\":%u}", 1);
    httpd_resp_send(req, ledJSON, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// An HTTP POST handler 
esp_err_t ledOff(httpd_req_t *req) {
    ESP_LOGI(TAG, "OFF LED"); 
    //gpio_set_level(GPIO_LED, !gpio_get_level(GPIO_LED));
    if (gpio_get_level(GPIO_LED) == 1) {gpio_set_level(GPIO_LED, 0);}
    httpd_resp_set_status(req, HTTPD_200);
    httpd_resp_set_type(req, "application/json");
    char ledJSON[100];
    sprintf(ledJSON, "{\"on_off_led\":%u}", 0);
    httpd_resp_send(req, ledJSON, strlen(ledJSON));
    return ESP_OK;
}

//--------------------------------------------------------------------------------------------------------------------
// An HTTP GET handler 
static esp_err_t image_handler(httpd_req_t *req){
    extern const char winter_start[] asm("_binary_winter_png_start");
    extern const char winter_end[] asm("_binary_winter_png_end");
    size_t winter_len = winter_end - winter_start;
    httpd_resp_set_type(req, "image/png");
    httpd_resp_send(req, winter_start, winter_len);
    return ESP_OK;
}
//--------------------------------------------------------------------------------------------------------------------
// Handler Error
esp_err_t module_HTTP::http_404_error_handler(httpd_req_t *req, httpd_err_code_t err){
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    } else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "URI not found");
    return ESP_FAIL;
}
//--------------------------------------------------------------------------------------------------------------------

httpd_handle_t module_HTTP::start_http_server(void){
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.max_open_sockets = 3;
    config.max_resp_headers = 5; // Максимальное количество пользовательских заголовков
    config.stack_size = 8192;// Максимальный размер стека, разрешенный для задачи сервера
    config.server_port = 80;// Номер порта TCP для приема и передачи трафика HTTP
    config.max_uri_handlers = 10;// Максимально допустимые URI обработчики
    config.lru_purge_enable = true;// Очистить соединение «Наименее недавно использованное»
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    basic_auth_info_t *basic_auth_info = (basic_auth_info_t*)calloc(1, sizeof(basic_auth_info_t));
    if (basic_auth_info) {
        basic_auth_info->username = CONFIG_EXAMPLE_BASIC_AUTH_USERNAME;
        basic_auth_info->password = CONFIG_EXAMPLE_BASIC_AUTH_PASSWORD;
    }else{
        ESP_LOGE(TAG, "Error starting server!");
        return NULL;
    }

    const httpd_uri_t index = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = module_HTTP::index_get_handler,
        .user_ctx  = basic_auth_info
    };
    const httpd_uri_t image_uri = {
        .uri       = "/winter.png",
        .method    = HTTP_GET,
        .handler   = image_handler,
        .user_ctx  = NULL
    };
    const httpd_uri_t ledOnUri = {
        .uri       = "/ledOn",
        .method    = HTTP_POST,
        .handler   = ledOn,
        .user_ctx  = basic_auth_info
    };
    const httpd_uri_t ledOffUri = {
        .uri       = "/ledOff",
        .method    = HTTP_POST,
        .handler   = ledOff,
        .user_ctx  = basic_auth_info
    };
    const httpd_uri_t h_json = {
        .uri       = "/command",
        .method    = HTTP_POST,
        .handler   = module_HTTP::json_post_handler,
        .user_ctx  = basic_auth_info 
    };
    const httpd_uri_t h_echo = {
        .uri       = "/echo",
        .method    = HTTP_POST,
        .handler   = module_HTTP::echo_post_handler,
        .user_ctx  = basic_auth_info 
    };

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server_httpd, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers:");
        ESP_LOGI(TAG, "POST: %s",h_json.uri);
        ESP_LOGI(TAG, "POST: %s",h_echo.uri);
        httpd_register_uri_handler(server_httpd, &index);
        httpd_register_uri_handler(server_httpd, &image_uri);
        httpd_register_uri_handler(server_httpd, &ledOnUri);
        httpd_register_uri_handler(server_httpd, &ledOffUri);
        httpd_register_uri_handler(server_httpd, &h_echo);
        httpd_register_uri_handler(server_httpd, &h_json);
        httpd_register_err_handler(server_httpd, HTTPD_404_NOT_FOUND, module_HTTP::http_404_error_handler);
        return server_httpd;
    }
    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
}

void module_HTTP::stop_http_server(){
    // Ensure handle is non NULL
    if (&server_httpd != NULL) {
        // Stop the httpd server
        httpd_stop(&server_httpd);
    }
}