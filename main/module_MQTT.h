
#include "mqtt_client.h"
#include "esp_ota_ops.h"
#include "esp_event.h"
#include <esp_log.h>

#define min(a,b) ((a)<(b)?(a):(b))

class module_MQTT
{
private:
    
public:
    module_MQTT();
    static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);
    static esp_err_t mqtt_app_start(const char *mqtt_broker_url, const char *mqtt_broker_auth_username, const char *mqtt_broker_auth_pass);
    static void send_binary(esp_mqtt_client_handle_t client);
    ~module_MQTT();
};