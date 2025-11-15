/* (c) 2025 M10tech
 * busdisplaybitdriver for ESP32
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_netif_sntp.h"
// #include "lcm_api.h"
#include <udplogger.h>
//#include "driver/uart.h"
//#include "soc/uart_reg.h"
#include "ping/ping_sock.h"
#include "hal/wdt_hal.h"
#include "mqtt_client.h"
#include "esp_ota_ops.h" //for esp_app_get_description
#include <cJSON.h>

// You must set version.txt file to match github version tag x.y.z for LCM4ESP32 to work

int display_idx=0;
char txt1[128],   txt2[128],   txt3[128];
int font1=0x58, font2=0x46, font3=0x46, layout=0x31, addr=0x33;
int mqtt_order=0;

static void log_error_if_nonzero(const char *message, int error_code) {if (error_code != 0) UDPLUS("Last error %s: 0x%x\n", message, error_code);}
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
//     UDPLUS("Event dispatched from event loop base=%s, event_id=%lx\n", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    const cJSON *json_txt1=NULL,*json_txt2=NULL,*json_txt3=NULL,*json_font1=NULL,*json_font2=NULL,*json_font3=NULL,*json_layout=NULL,*json_addr=NULL;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        UDPLUS("MQTT_EVENT_CONNECTED\n");
        msg_id = esp_mqtt_client_subscribe(client, "bus_panel/bitmap", 0);
        UDPLUS("sent subscribe successful, msg_id=%d\n", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        UDPLUS("MQTT_EVENT_DISCONNECTED\n");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        UDPLUS("MQTT_EVENT_SUBSCRIBED, msg_id=%d\n", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        //UDPLUS( "MQTT_EVENT_DATA\n");
        UDPLUS("TOPIC=%.*s DATA=\n", event->topic_len, event->topic);
        UDPLUS("%.*s\n", event->data_len, event->data);
        cJSON *json = cJSON_ParseWithLength(event->data, event->data_len);
        if (json) {
//             char *myJson = cJSON_Print(json);
//             UDPLUS("JSON: %s\n",myJson);
//             free(myJson);
            json_txt1 = cJSON_GetObjectItemCaseSensitive(json, "txt1");
            if (cJSON_IsString(json_txt1) && (json_txt1->valuestring != NULL)) {
                UDPLUS("Text 1 is \"%s\"\n", json_txt1->valuestring);
                strcpy(txt1,json_txt1->valuestring);
            }
            json_txt2 = cJSON_GetObjectItemCaseSensitive(json, "txt2");
            if (cJSON_IsString(json_txt2) && (json_txt2->valuestring != NULL)) {
                UDPLUS("Text 2 is \"%s\"\n", json_txt2->valuestring);
                strcpy(txt2,json_txt2->valuestring);
            }
            json_txt3 = cJSON_GetObjectItemCaseSensitive(json, "txt3");
            if (cJSON_IsString(json_txt3) && (json_txt3->valuestring != NULL)) {
                UDPLUS("Text 3 is \"%s\"\n", json_txt3->valuestring);
                strcpy(txt3,json_txt3->valuestring);
            }
            json_font1 = cJSON_GetObjectItemCaseSensitive(json, "font1");
            if (cJSON_IsNumber(json_font1)) {
                UDPLUS("Font 1 is \"0x%02x\"\n", json_font1->valueint);
                font1=json_font1->valueint;
            }
            json_font2 = cJSON_GetObjectItemCaseSensitive(json, "font2");
            if (cJSON_IsNumber(json_font2)) {
                UDPLUS("Font 2 is \"0x%02x\"\n", json_font2->valueint);
                font2=json_font2->valueint;
            }
            json_font3 = cJSON_GetObjectItemCaseSensitive(json, "font3");
            if (cJSON_IsNumber(json_font3)) {
                UDPLUS("Font 3 is \"0x%02x\"\n", json_font3->valueint);
                font3=json_font3->valueint;
            }
            json_layout = cJSON_GetObjectItemCaseSensitive(json, "layout");
            if (cJSON_IsNumber(json_layout)) {
                UDPLUS("Layout is \"0x%02x\"\n", json_layout->valueint);
                layout=json_layout->valueint;
            }
            json_addr = cJSON_GetObjectItemCaseSensitive(json, "addr");
            if (cJSON_IsNumber(json_addr)) {
                UDPLUS("Address = \"0x%02x\"\n", json_addr->valueint);
                addr=json_addr->valueint;
            }
            cJSON_Delete(json);
            mqtt_order=1;
        } else {
        }
        break;
    case MQTT_EVENT_ERROR:
        UDPLUS("MQTT_EVENT_ERROR\n");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            UDPLUS("Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        UDPLUS("Other event id:%d", event->event_id);
        break;
    }
}

char *broker_uri=NULL;
static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg={};
    if (broker_uri==NULL) {
        char line[128];
        int count = 0;
        UDPLUS("Please enter url of mqtt broker\n");
        while (count < 127) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        UDPLUS("Broker url: %s\n", line);
    } else {
        mqtt_cfg.broker.address.uri = broker_uri;
    }
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

char    *pinger_target=NULL;

wdt_hal_context_t rtc_wdt_ctx = RWDT_HAL_CONTEXT_DEFAULT(); //RTC WatchDogTimer context
int ping_count=60,ping_delay=1; //seconds
static void ping_success(esp_ping_handle_t hdl, void *args) {
    ping_count+=20;
    if (ping_count>120) ping_count=120;
    //uint32_t elapsed_time;
    //ip_addr_t response_addr;
    //esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    //esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR,  &response_addr,  sizeof(response_addr));
    //UDPLUS("good ping from %s %lu ms -> count: %d s\n", inet_ntoa(response_addr.u_addr.ip4), elapsed_time, ping_count);
    //feed the RTC WatchDog
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
}
static void ping_timeout(esp_ping_handle_t hdl, void *args) {
    //ping_count--; ping_delay=1;
    //UDPLUS("failed ping -> count: %d s\n", ping_count);
    //feed the RTC WatchDog ANYHOW, until the code is changed to ping the default gateway automatically
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_feed(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);
}
void ping_task(void *argv) {
    ip_addr_t target_addr;
    ipaddr_aton(pinger_target,&target_addr);
    esp_ping_handle_t ping;
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;
    ping_config.timeout_ms = 900; //should time-out before our 1s delay
    ping_config.count = 1; //one ping at a time means we can regulate the interval at will
    esp_ping_callbacks_t cbs = {.on_ping_success=ping_success, .on_ping_timeout=ping_timeout, .on_ping_end=NULL, .cb_args=NULL}; //set callback functions
    esp_ping_new_session(&ping_config, &cbs, &ping);
    
    UDPLUS("Pinging IP %s\n", ipaddr_ntoa(&target_addr));
    //re-enable RTC WatchDogTimer (don't depend on bootloader to not disable it)
    wdt_hal_write_protect_disable(&rtc_wdt_ctx);
    wdt_hal_enable(&rtc_wdt_ctx);
    wdt_hal_write_protect_enable(&rtc_wdt_ctx);    
    while(ping_count){
        vTaskDelay((ping_delay-1)*(1000/portTICK_PERIOD_MS)); //already waited 1 second...
        esp_ping_start(ping);
        vTaskDelay(1000/portTICK_PERIOD_MS); //waiting for answer or timeout to update ping_delay value
    }
    UDPLUS("restarting because can't ping home-hub\n");
    vTaskDelay(1000/portTICK_PERIOD_MS); //allow UDPlog to flush output
    esp_restart(); //TODO: disable GPIO outputs
}

void time_task(void *argv) {
    setenv("TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 1); tzset();
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
        UDPLUS("Still waiting for system time to sync\n");
    }
    time_t ts = time(NULL);
    UDPLUS("TIME SET: %u=%s\n", (unsigned int) ts, ctime(&ts));
    vTaskDelete(NULL);
}

char localhost[]="127.0.0.1";
static void ota_string() {
    char *display_nr=NULL;
    esp_err_t status;
    nvs_handle_t lcm_handle;
    char *otas=NULL;
    size_t  size;
    status = nvs_open("LCM", NVS_READONLY, &lcm_handle);
    
    if (!status && nvs_get_str(lcm_handle, "ota_string", NULL, &size) == ESP_OK) {
        otas = malloc(size);
        nvs_get_str(lcm_handle, "ota_string", otas, &size);
        display_nr=strtok(otas,";");
        broker_uri=strtok(NULL,";");
        pinger_target=strtok(NULL,";");
    }
    if (display_nr==NULL) display_idx=0; else display_idx=atoi(display_nr);
    if (pinger_target==NULL) pinger_target=localhost;
    //DO NOT free the otas since it carries the config pieces
}


void main_task(void *arg) {
    udplog_init(3);
    vTaskDelay(300); //Allow Wi-Fi to connect
    UDPLUS("\n\nBus-Panel-BitDriver %s\n",esp_app_get_description()->version);

//     nvs_handle_t lcm_handle;nvs_open("LCM", NVS_READWRITE, &lcm_handle);nvs_set_str(lcm_handle,"ota_string", "3;mqtt://busdisplay:notthesecret@192.168.178.5;192.168.178.5");
//     nvs_commit(lcm_handle); //can be used if not using LCM
    ota_string();

    xTaskCreate(time_task,"Time", 2048, NULL, 6, NULL);
    xTaskCreate(ping_task,"PingT",2048, NULL, 1, NULL);

    mqtt_app_start();

    while (true) {
        vTaskDelay(10);
    }
}    

void app_main(void) {
    printf("app_main-start\n");

    //The code in this function would be the setup for any app that uses wifi which is set by LCM
    //It is all boilerplate code that is also used in common_example code
    esp_err_t err = nvs_flash_init(); // Initialize NVS
    if (err==ESP_ERR_NVS_NO_FREE_PAGES || err==ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase()); //NVS partition truncated and must be erased
        err = nvs_flash_init(); //Retry nvs_flash_init
    } ESP_ERROR_CHECK( err );

    //TODO: if no wifi setting found, trigger otamain
    
    //block that gets you WIFI with the lowest amount of effort, and based on FLASH
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_config.route_prio = 128;
    esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    //end of boilerplate code

    xTaskCreate(main_task,"main",4096,NULL,1,NULL);
    while (true) {
        vTaskDelay(1000); 
    }
    printf("app_main-done\n"); //will never exit here
}
