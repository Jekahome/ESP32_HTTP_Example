#include "module_MQTT.h"

static const char *TAG = "Client MQTT";
 
//--------------------------------------------------------------------------------
/*

    Запустить брокер на хосте:
    $ mosquitto -c custom.conf

    Запустить прослушивание topic /topic/esp32_to_all на хосте:
    $ mosquitto_sub -t '/topic/esp32_to_all'

    Узнать IP адрес хоста
    $ ifconfig
    192.168.1.167 => "mqtt://192.168.1.167"

    Настроить подключение к брокеру: 
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL, // mqtt://mqtt.eclipseprojects.io
    };

    Отправить в topic /topic/esp32_to_all сообщение:
    int msg_id = esp_mqtt_client_publish(client, "/topic/esp32_to_all", "Hello .....!", 0, 1, 0);

    Подписаться на /topic/all_to_esp32
    int msg_id = esp_mqtt_client_subscribe(client, "/topic/all_to_esp32", 0);

    Отправка c хоста в topic сообщения 
    $ mosquitto_pub -t '/topic/all_to_esp32' -m 'Hello ESP32'

*/

module_MQTT::module_MQTT(){}
module_MQTT::~module_MQTT(){ ESP_LOGW(TAG,"Client GOODBAY"); }

char *get_decrypt(char *src);
char *get_encrypt(char *src);


//-------------------------------------------------------------------------------
/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
void module_MQTT::mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
  
        // Отправка сообщения
        msg_id = esp_mqtt_client_publish(client, "/topic/esp32_to_all", "Hello .....!", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
       
        // Подписка
        msg_id = esp_mqtt_client_subscribe(client, "/topic/all_to_esp32", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        // Отписка
        //msg_id = esp_mqtt_client_unsubscribe(client, "/topic/all_to_esp32");
        //ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
         
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        // При получении данных по подписке
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        if (strncmp(event->data, "send binary please", event->data_len) == 0) {
            ESP_LOGI(TAG, "Sending the binary");
            send_binary(client);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x", event->error_handle->esp_tls_last_esp_err);
            ESP_LOGI(TAG, "Last tls stack error number: 0x%x", event->error_handle->esp_tls_stack_err);
            ESP_LOGI(TAG, "Last captured errno : %d (%s)",  event->error_handle->esp_transport_sock_errno,
                     strerror(event->error_handle->esp_transport_sock_errno));
        } else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
            ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
        } else {
            ESP_LOGW(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//
void module_MQTT::send_binary(esp_mqtt_client_handle_t client){
    spi_flash_mmap_handle_t out_handle;
    const void *binary_address;
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &binary_address, &out_handle);
    // sending only the configured portion of the partition (if it's less than the partition size)
    int binary_size = min(CONFIG_BROKER_BIN_SIZE_TO_SEND, partition->size);
    int msg_id = esp_mqtt_client_publish(client, "/topic/binary", (char *)binary_address, binary_size, 0, 0);
    ESP_LOGI(TAG, "binary sent with msg_id=%d", msg_id);
}

esp_err_t module_MQTT::mqtt_app_start(const char *mqtt_broker_url, const char *mqtt_broker_auth_username, const char *mqtt_broker_auth_pass){
    
    // TODO:протокол MQTT пересылает незашифрованные заголовки c логином и паролем, в отличии от MQTTS
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = mqtt_broker_url; 
    mqtt_cfg.credentials.username =  mqtt_broker_auth_username;
    mqtt_cfg.credentials.client_id = "J1-";
    mqtt_cfg.credentials.authentication.password = mqtt_broker_auth_pass; 
    ESP_LOGI(TAG, "SETTINGS MQTT: URL:%s USER:%s PASS:%s",mqtt_broker_url,mqtt_broker_auth_username,mqtt_broker_auth_pass);

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    // The last argument may be used to pass data to the event handler, in this example mqtt_event_handler 
    esp_err_t err = esp_mqtt_client_register_event(client,(esp_mqtt_event_id_t)MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    if(ESP_OK != err) {
       ESP_LOGE(TAG, "ERROR REGISTER MQTT EVENT: %s", esp_err_to_name(err));
       vTaskDelay(2000/portTICK_PERIOD_MS);
	   return err;
	}
    err = esp_mqtt_client_start(client);
    if(ESP_OK != err) {
       ESP_LOGE(TAG, "ERROR MQTT START: %s", esp_err_to_name(err));
       vTaskDelay(2000/portTICK_PERIOD_MS);
	   return err;
	}
    return ESP_OK;
}
