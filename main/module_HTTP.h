#include <string>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "cJSON.h" 
#include "esp_netif.h"
#include <stdint.h> // SIZE_MAX 

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#include "esp_tls_crypto.h"
#include "driver/gpio.h"
#include "esp_tls.h"

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

#define min(a,b) ((a)<(b)?(a):(b))

enum Command
{
    succeess_picture = 1,
    succeess_sound = 2,
    send_picture = 3,
    motor_user_control = 4,
    motor_auto_control = 5,
    say = 6,
    error_message = 7,
    invalid_command = 8,
    get_sentence = 9,
    succeess_sentence = 10,
    set_sentence = 11,
    msg_log = 12,
    esp32cam_restart = 13
};

class module_HTTP
{
private:
    
public:
    module_HTTP();
    
    // HTTP Server
    httpd_handle_t start_http_server();
    void stop_http_server();
    static esp_err_t json_post_handler(httpd_req_t *req);
    static esp_err_t echo_post_handler(httpd_req_t *req);
    static esp_err_t index_get_handler(httpd_req_t *req);
    static char* http_auth_basic(const char *username, const char *password);
    static bool check_auth(httpd_req_t *req);
    static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err);
    ~module_HTTP();
};
