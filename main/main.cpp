#include <stdio.h>
#include <esp_log.h>
#include "spi_flash_mmap.h"
#include "esp_check.h"
#include <string.h> // for `memset`
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/sys.h"
#include "esp_http_server.h"

// TCP
#include <sys/param.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "sys/socket.h"
#include "netdb.h"
#include "errno.h"

#include "module_HTTP.h"

// --------------------------------------------------
#include "mqtt_client.h"
#include "esp_ota_ops.h"
//---------------------------------------------------

/*
  Run:

  idf.py build
  idf.py -p /dev/ttyUSB0 -b 115200 flash
  idf.py -p /dev/ttyUSB0 monitor 
*/

#define MQTT_BROKER_URL             (const char *) CONFIG_MQTT_BROKER_URL
#define MQTT_BROKER_AUTH_USERNAME   (const char *) CONFIG_MQTT_BROKER_AUTH_USERNAME
#define MQTT_BROKER_AUTH_PASSWORD   (const char *) CONFIG_MQTT_BROKER_AUTH_PASSWORD
// WiFi -----------------------------------------------------------------------------
#define EXAMPLE_ESP_WIFI_SSID     (const char *) CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      (const char *) CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY
#define EXAMPLE_ESP_WIFI_CHANNEL   CONFIG_ESP_WIFI_CHANNEL

// Static IP ------------------------------------------------------------------------
#define EXAMPLE_STATIC_IP_ADDR        CONFIG_EXAMPLE_STATIC_IP_ADDR
#define EXAMPLE_STATIC_NETMASK_ADDR   CONFIG_EXAMPLE_STATIC_NETMASK_ADDR
#define EXAMPLE_STATIC_GW_ADDR        CONFIG_EXAMPLE_STATIC_GW_ADDR
#ifdef CONFIG_EXAMPLE_STATIC_DNS_AUTO
#define EXAMPLE_MAIN_DNS_SERVER       EXAMPLE_STATIC_GW_ADDR
#define EXAMPLE_BACKUP_DNS_SERVER     "0.0.0.0"
#else
#define EXAMPLE_MAIN_DNS_SERVER       CONFIG_EXAMPLE_STATIC_DNS_SERVER_MAIN
#define EXAMPLE_BACKUP_DNS_SERVER     CONFIG_EXAMPLE_STATIC_DNS_SERVER_BACKUP
#endif
#ifdef CONFIG_EXAMPLE_STATIC_DNS_RESOLVE_TEST
#define EXAMPLE_RESOLVE_DOMAIN        CONFIG_EXAMPLE_STATIC_RESOLVE_DOMAIN
#endif

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define URL_HTTP_REMOTE "http://192.168.0.104:4011/sound"

// Pin Interrupt 
#define GPIO_INTERRUPT_RECORD  gpio_num_t::GPIO_NUM_18// 5
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "wifi station";
static int s_retry_num = 0;
// FreeRTOS event group to signal when we are connected
static EventGroupHandle_t s_wifi_event_group;
esp_netif_t * netif_t;
DMA_ATTR static module_HTTP *server_HTTP;
 
extern "C" {
   void app_main(void); 
}
 
static void set_static_ip(){
    if (esp_netif_dhcpc_stop(netif_t) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
        return;
    }
    esp_netif_ip_info_t ip;
    memset(&ip, 0 , sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(EXAMPLE_STATIC_IP_ADDR);
    ip.netmask.addr = ipaddr_addr(EXAMPLE_STATIC_NETMASK_ADDR);
    ip.gw.addr = ipaddr_addr(EXAMPLE_STATIC_GW_ADDR);
    if (esp_netif_set_ip_info(netif_t,&ip) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }
    ESP_LOGI(TAG, "Success to set static ip: %s, netmask: %s, gw: %s", EXAMPLE_STATIC_IP_ADDR, EXAMPLE_STATIC_NETMASK_ADDR, EXAMPLE_STATIC_GW_ADDR);
   //ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(EXAMPLE_MAIN_DNS_SERVER), ESP_NETIF_DNS_MAIN));
   //ESP_ERROR_CHECK(set_dns_server(netif, ipaddr_addr(EXAMPLE_BACKUP_DNS_SERVER), ESP_NETIF_DNS_BACKUP));
}

// Обработчик события WiFi 
static void event_handler(void* arg, esp_event_base_t event_base,int32_t event_id, void* event_data){
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-disconnect-phase
    switch (event_id)
    {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi connecting ... \n"); 
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi connected ... \n"); 
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WiFi clost connection ... \n"); 
            break; 
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "WiFi got IP ... \n"); 
            break;    
        default:
            break;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        set_static_ip();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }

  if (event_base == IP_EVENT) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t* ip_info = &event->ip_info;
    switch (event_id) {
      case IP_EVENT_ETH_GOT_IP:
        ESP_LOGI(TAG, "Ethernet Got IP Address");
        ESP_LOGI(TAG, "\e[0m \e[1m\e[38;5;9m%s\e[0m\n", "~~~~~~~~~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "ETH IP: " IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "ETH MASK: " IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "ETH GW: " IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "\e[0m \e[1m\e[38;5;9m%s\e[0m\n", "~~~~~~~~~~~~~~~~~~~~~~");
        //xEventGroupSetBits(net_event_group, NET_CONNECTED_BIT);
        break;
      case IP_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "WiFi Got IP Address");
        ESP_LOGI(TAG, "\e[0m \e[1m\e[38;5;9m%s\e[0m\n", "~~~~~~~~~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "WIFI IP: " IPSTR, IP2STR(&ip_info->ip));
        ESP_LOGI(TAG, "WIFI MASK: " IPSTR, IP2STR(&ip_info->netmask));
        ESP_LOGI(TAG, "WIFI GW: " IPSTR, IP2STR(&ip_info->gw));
        ESP_LOGI(TAG, "\e[0m \e[1m\e[38;5;9m%s\e[0m\n", "~~~~~~~~~~~~~~~~~~~~~~");
        s_retry_num = 0;
        //xEventGroupSetBits(net_event_group, NET_CONNECTED_BIT);
        break;
    }
  }
}

// Режим только станции
void wifi_init_sta(void){
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());// TCP/IP init. создание основной задачи LwIP
    esp_netif_ip_info_t ipInfo;
    ESP_ERROR_CHECK(esp_event_loop_create_default());// инициализацию функции обратного вызова события приложения
    netif_t = esp_netif_create_default_wifi_sta();// создает станцию ​​привязки экземпляра сетевого интерфейса со стеком TCP/IP
    assert(netif_t);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));// создание задачи драйвера Wi-Fi и инициализацию драйвера Wi-Fi

    // [2 WiFi Configuration Phase.]
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        netif_t,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        netif_t,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {};
    wifi_config.sta = {};
  	strcpy ((char *) wifi_config.sta.ssid, EXAMPLE_ESP_WIFI_SSID);
	strcpy ((char *) wifi_config.sta.password, EXAMPLE_ESP_WIFI_PASS);
	 /* 
    .threshold.authmode
         Установка пароля подразумевает, что станция будет подключаться ко всем режимам безопасности, включая WEP/WPA.
         Однако эти режимы устарели и не рекомендуется использовать. 
		 Сделайте свою точку доступа не поддерживает WPA2, этот режим можно включить,закомментировав строку
	 */
    //wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg = {};
  	wifi_config.sta.pmf_cfg.capable = true;
	wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );// тип сети Станция
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    // [3 WiFi Start Phase.]
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_netif_get_ip_info(netif_t, &ipInfo));

    //ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Ожидание, пока соединение не будет установлено (WIFI_CONNECTED_BIT) или соединение не удастся максимально количество повторных попыток (WIFI_FAIL_BIT). 
	   Биты устанавливаются функцией event_handler() (см. Выше) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    // xEventGroupWaitBits() возвращает биты перед возвратом вызова, поэтому мы можем проверить, какое событие на самом деле произошло. 
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID:%s, password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    // Событие не будет обработано после отмены регистрации
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

esp_err_t setup(){ 
    esp_err_t ret = ESP_FAIL;
   
   /*
    Чтобы установить подключение к Интернету с помощью ESP32, нам необходимо выполнить три шага:
    https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/wifi.html#wi-fi-lwip-init-phase
    1. Инициализировать стек TCP / IP ESP-IDF
    2. Инициализировать драйвер Wi-Fi ESP-IDF в режиме станции со статическим IP
    3. Подключитесь к сети Wi-Fi
   */
 
    // [1 WiFi Step.] Wi-Fi/LwIP Init Phase
    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
    ESP_LOGI(TAG,"\e[0m >>>>>> \e[1m\e[38;5;11m\e[48;5;0m%s\e[0m <<<<<<<<\n", "WiFi run!" );
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    return ret;
} 
 
//--------------------------------------------------------------------------------
/*
    Запустить прослушивание topic на ПК:
    $ mosquitto_sub -h mqtt.eclipseprojects.io -t '/my_topic/qos1'
*/
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
//
// Note: this function is for testing purposes only publishing part of the active partition
//       (to be checked against the original binary)
//
static void send_binary(esp_mqtt_client_handle_t client)
{
    spi_flash_mmap_handle_t out_handle;
    const void *binary_address;
    const esp_partition_t *partition = esp_ota_get_running_partition();
    esp_partition_mmap(partition, 0, partition->size, SPI_FLASH_MMAP_DATA, &binary_address, &out_handle);
    // sending only the configured portion of the partition (if it's less than the partition size)
    int binary_size = MIN(CONFIG_BROKER_BIN_SIZE_TO_SEND, partition->size);
    int msg_id = esp_mqtt_client_publish(client, "/topic/binary", (char *)binary_address, binary_size, 0, 0);
    ESP_LOGI(TAG, "binary sent with msg_id=%d", msg_id);
}

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
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
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
        /*msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        */
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

 
static esp_err_t mqtt_app_start(void){
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URL;// MQTT_BROKER_URL, mqtt://192.168.1.167, mqtt://mqtt.eclipseprojects.io
    mqtt_cfg.credentials.username = MQTT_BROKER_AUTH_USERNAME;
    mqtt_cfg.credentials.authentication.password = MQTT_BROKER_AUTH_PASSWORD; 
     
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


//--------------------------------------------------------------------------------
void app_main(void){
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set(TAG, ESP_LOG_VERBOSE);

    ESP_LOGI(TAG,"\nWiFi!\n\n");
    esp_err_t err = setup();
    if(ESP_OK != err) {
       ESP_LOGE(TAG, "ERROR SETUP: %s", esp_err_to_name(err));
       vTaskDelay(2000/portTICK_PERIOD_MS);
	   return;
	}

    ESP_LOGI(TAG,"ESP32 Wemos D1 R32 - це плата у форматі Arduino Uno на основі WiFi Bluetooth модуля WROOM-32");
    ESP_LOGI(TAG,"This is esp32 chip with 2 CPU core(s), WiFi/BT/BLE, silicon revision 1, 4MB external flash");
    ESP_LOGW(TAG,"PSRAM должен быть отключен в menuconfig\n");
    vTaskDelay(2000/portTICK_PERIOD_MS);

/*
    server_HTTP = new module_HTTP();
    //server_HTTP->start_http_webserver();
    server_HTTP->start_https_webserver();
 */
    
    // MQTT client
    mqtt_app_start();

    ESP_LOGI( TAG,  "\e[0m \e[1m\e[38;5;9m>\e[0m\e[1m\e[38;5;12m>\e[0m\e[1m\e[38;5;11m>\e[0m\e[1m\e[38;5;14m>\e[0m\e[1m\e[38;5;5m>\e[0m\e[1m\e[38;5;10m>\e[0m \e[1m\e[38;5;11m\e[48;5;0m%s\e[0m \e[1m\e[38;5;9m<\e[0m\e[1m\e[38;5;12m<\e[0m\e[1m\e[38;5;11m<\e[0m\e[1m\e[38;5;14m<\e[0m\e[1m\e[38;5;5m<\e[0m\e[1m\e[38;5;10m<\e[0m\n", 
    "Yippee, build completed successfully");
    fflush(stdout);  
}



 

/*
Чтобы улучшить пропускную способность, вы можете попробовать изменить некоторые конфигурации в menuconfig, как показано ниже:

 CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM=16
 CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=64
 CONFIG_ESP32_WIFI_DYNAMIC_TX_BUFFER_NUM=64
 CONFIG_ESP32_WIFI_TX_BA_WIN=32
 CONFIG_ESP32_WIFI_RX_BA_WIN=32
 CONFIG_LWIP_TCP_SND_BUF_DEFAULT=65534
 CONFIG_LWIP_TCP_WND_DEFAULT=65534
 CONFIG_LWIP_TCP_RECVMBOX_SIZE=64
 CONFIG_LWIP_UDP_RECVMBOX_SIZE=64
 CONFIG_LWIP_TCPIP_RECVMBOX_SIZE=64
 
Обратите внимание, что эти изменения также увеличат потребление памяти.

*/