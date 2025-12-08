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
#include "driver/i2s_std.h"
// #include "lcm_api.h"
#include <udplogger.h>
//#include "driver/uart.h"
//#include "soc/uart_reg.h"
#include "ping/ping_sock.h"
#include "hal/wdt_hal.h"
#include "mqtt_client.h"
#include "esp_ota_ops.h" //for esp_app_get_description
#include <cJSON.h>
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/pulse_cnt.h"

//#include <math.h>
double sqroot(double square){ //Newton Raphson
    if (square<=0) return 0;
    double root=square/3;
    for (int i=0; i<16; i++) root=(root+square/root)/2;
    return root;
}

// You must set version.txt file to match github version tag x.y.z for LCM4ESP32 to work

//select your movie, it must set COLUMNS, FRAMES anf FPS
//#include "demo.h" //block pattern running up
#include "bad_apple.h"
#define FRAMETIME (1000000/FPS) // in microseconds
#define FINETUNE 109/100 //slow down by 9% to better reach 30 fps

#define LGHT_PIN  GPIO_NUM_13
#define BLNK_PIN  GPIO_NUM_12
#define XLAT_PIN  GPIO_NUM_14
#define SCLK_PIN  GPIO_NUM_27
#define SINT_PIN  GPIO_NUM_26
#define SINB_PIN  GPIO_NUM_25

// gpio_set_level(SCLK_PIN,ARMIT);                      //prepare rise by going to low
// gpio_set_level(SINT_PIN,((data&b    )== b    )?1:0); //trigger SIN bit in the TOP
// gpio_set_level(SINB_PIN,((data&b<<16)== b<<16)?1:0); //trigger SIN bit in the BOTTOM
// gpio_set_level(SCLK_PIN,SHIFT);                      //shift on rising edge

#define ARMIT 0 //to arm the clock, we set it low
#define SHIFT 1 //to shift the data in, we set the clock high for a rising edge
#define DELAY 1 //asm_loops or microseconds

//#define DELAYIT(t)   do { vTaskDelay(t);} while(0)
//#define DELAYIT(t)   do { start_time=esp_timer_get_time();while(((uint64_t)esp_timer_get_time()-start_time)<t){} } while(0) //takes 10750 microseconds per DELAYIT(10000)
#define DELAYIT(t)     do { int delayt=t; \
                            __asm__ __volatile__ (  "start%=:   addi.n  %0, %0, -1\n"  \
                                                    "           bnez.n    %0, start%=" \
                                                    : "=r"(delayt):"0"(delayt)      ); \
                          } while(0) //inline assembly loop is slighty faster than function version
#define ONALLBITS(f,b,d) do{gpio_set_level(SCLK_PIN,ARMIT ); \
                            gpio_set_level(SINB_PIN,((screen[f][col]&b    )== b    )?1:0); \
                            gpio_set_level(SINT_PIN,((screen[f][col]&b<<16)== b<<16)?1:0); \
                            DELAYIT(d); \
                            gpio_set_level(SCLK_PIN,SHIFT); \
                            DELAYIT(DELAY); \
                           } while(0) //check for a full match to set the SIN bit
#define ONSOMEBIT(f,b,d) do{gpio_set_level(SCLK_PIN,ARMIT ); \
                            gpio_set_level(SINB_PIN,(screen[f][col]&b    )?1:0); \
                            gpio_set_level(SINT_PIN,(screen[f][col]&b<<16)?1:0); \
                            DELAYIT(d); \
                            gpio_set_level(SCLK_PIN,SHIFT); \
                            DELAYIT(DELAY); \
                           } while(0) //check for some match to set the SIN bit

#define LOAD 0 //XLAT low  means to hide the new values to the LEDs and keep the old values on the LEDs
#define SHOW 1 //XLAT high means to pass the new values to the LEDs
portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
static void show_frame_once(int frame) {
    //uint64_t start_time; //used in the DELAYIT macro based on esp_timer_get_time()
    int col;
    gpio_set_level(XLAT_PIN,LOAD);
    for (col=0;col<COLUMNS;col++) { //iterate over each column to match value 0b11
        ONALLBITS(frame,0x00000003,DELAY);
        ONALLBITS(frame,0x0000000c,DELAY);
        ONALLBITS(frame,0x00000030,DELAY);
        ONALLBITS(frame,0x000000c0,DELAY);
        ONALLBITS(frame,0x00000300,DELAY);
        ONALLBITS(frame,0x00000c00,DELAY);
        ONALLBITS(frame,0x00003000,DELAY);
        ONALLBITS(frame,0x0000c000,DELAY);
    }
	taskENTER_CRITICAL(&myMutex);
    gpio_set_level(XLAT_PIN,SHOW);
    DELAYIT(DELAY);
    gpio_set_level(XLAT_PIN,LOAD);
	taskEXIT_CRITICAL(&myMutex);
    for (col=0;col<COLUMNS;col++) { //iterate over each column to match value 0b11 or 0b10
        ONALLBITS(frame,0x00000002,DELAY*400*FINETUNE);
        ONALLBITS(frame,0x0000000a,DELAY*400*FINETUNE);
        ONALLBITS(frame,0x00000020,DELAY*400*FINETUNE);
        ONALLBITS(frame,0x000000a0,DELAY*400*FINETUNE);
        ONALLBITS(frame,0x00000200,DELAY*400*FINETUNE);
        ONALLBITS(frame,0x00000a00,DELAY*400*FINETUNE);
        ONALLBITS(frame,0x00002000,DELAY*400*FINETUNE);
        ONALLBITS(frame,0x0000a000,DELAY*400*FINETUNE);
    }
	taskENTER_CRITICAL(&myMutex);
    gpio_set_level(XLAT_PIN,SHOW);
    DELAYIT(DELAY);
    gpio_set_level(XLAT_PIN,LOAD);
	taskEXIT_CRITICAL(&myMutex);
    for (col=0;col<COLUMNS;col++) { //iterate over each column to match value 0b11, 0b10 or 0b01
        ONSOMEBIT(frame,0x00000003,DELAY*100*FINETUNE);
        ONSOMEBIT(frame,0x0000000c,DELAY*100*FINETUNE);
        ONSOMEBIT(frame,0x00000030,DELAY*100*FINETUNE);
        ONSOMEBIT(frame,0x000000c0,DELAY*100*FINETUNE);
        ONSOMEBIT(frame,0x00000300,DELAY*100*FINETUNE);
        ONSOMEBIT(frame,0x00000c00,DELAY*100*FINETUNE);
        ONSOMEBIT(frame,0x00003000,DELAY*100*FINETUNE);
        ONSOMEBIT(frame,0x0000c000,DELAY*100*FINETUNE);
    }
	taskENTER_CRITICAL(&myMutex);
    gpio_set_level(XLAT_PIN,SHOW);
    DELAYIT(DELAY);
    gpio_set_level(XLAT_PIN,LOAD);
	taskEXIT_CRITICAL(&myMutex);
}


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
            UDPLUS("Last errno string (%s)\n", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        UDPLUS("Other event id:%d\n", event->event_id);
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
static void ping_task(void *argv) {
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

static void time_task(void *argv) {
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

static void init_gpio() {
    gpio_config_t io_conf = {}; //zero-initialize the config structure
    io_conf.intr_type = GPIO_INTR_DISABLE; //disable interrupt
    io_conf.mode = GPIO_MODE_OUTPUT; //set as output mode
    io_conf.pin_bit_mask = ((1ULL<<XLAT_PIN)|(1ULL<<SCLK_PIN)|(1ULL<<SINT_PIN)|(1ULL<<SINB_PIN));
    gpio_config(&io_conf); //configure GPIOs with the given settings
    gpio_set_level(XLAT_PIN,0);
    gpio_set_level(SCLK_PIN,0);
    gpio_set_level(SINT_PIN,0);
    gpio_set_level(SINB_PIN,0);
}

#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_MODE       LEDC_LOW_SPEED_MODE
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_RESOLUTION LEDC_TIMER_10_BIT // Set duty resolution to 10 bits, values 0-1024
static void ledc_init(void) {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_RESOLUTION,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = 3000,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = BLNK_PIN,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

uint32_t dutycycle=0;
uint32_t overflow_counter=0;
#define MIN_DUTYCYCLE 14 // 0/00
#define MAX_DUTYCYCLE 1<<LEDC_RESOLUTION
#define REACT 5 //time (-1) it will take for a stable change to be detected
#define N (2*REACT+1) //samples in the ringbuffer and seconds of the sliding window
uint32_t ring[N]={0,0,0,0,0,0,0,0,0,0,0}; //TODO: initialize dynamicly if N changes
uint32_t sort[N]={0,0,0,0,0,0,0,0,0,0,0};
void pcnt_task(void *arg) {
    uint32_t old,new=0;
    int i,in,io,idx=0;
    while(true) {
        ledc_set_duty(   LEDC_MODE,LEDC_CHANNEL,dutycycle);
        ledc_update_duty(LEDC_MODE,LEDC_CHANNEL);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        new=overflow_counter; //TODO: divide by passed time
        old=ring[idx]; ring[idx]=new;
        if (new<old) {  //search where to insert new measurement into sorted array
            i=0;   while (new>sort[i]) {i++;} in=i;
                   while (old>sort[i]) {i++;} io=i;
            for (i=io;i>in;i--) sort[i]=sort[i-1];
            sort[i]=new;
        }
        // if (new==old) do_nothing;
        if (new>old) {
            i=N-1; while (new<sort[i]) {i--;} in=i;
                   while (old<sort[i]) {i--;} io=i;
            for (i=io;i<in;i++) sort[i]=sort[i+1];
            sort[i]=new;
        }
        idx++; if(idx==N) idx=0;
        //for (i=0;i<N;i++) UDPLUS("%ld ",sort[i]); UDPLUS("\n");
        //convert overflow_counter to duty_cycle
        dutycycle=sqroot(sort[REACT])*10; if(dutycycle<MIN_DUTYCYCLE) dutycycle=MIN_DUTYCYCLE; if(dutycycle>MAX_DUTYCYCLE) dutycycle=MAX_DUTYCYCLE;
        UDPLUS("Median: %ld   now: %ld, dutycycle=%ld\n", sort[REACT], new, dutycycle);
        overflow_counter=0;
    }
}

static bool pcnt_on_overflow(pcnt_unit_handle_t unit, const pcnt_watch_event_data_t *edata, void *user_ctx) {
    uint32_t *overflow_counter=(uint32_t*)user_ctx;
    (*overflow_counter)++; //don't forget the ()
    return pdFALSE;
}

#define HIGH_LIMIT 100 //value is signed 16 bit so 32767 max
void pcnt_init(void) {
    pcnt_unit_config_t unit_config = {
        .high_limit = HIGH_LIMIT,
        .low_limit = -1, //the only way is up
    };
    pcnt_unit_handle_t pcnt_unit = NULL;
    pcnt_new_unit(&unit_config, &pcnt_unit);
    pcnt_chan_config_t chan_config = {
        .edge_gpio_num = LGHT_PIN,
        .level_gpio_num = -1,
        .flags.virt_level_io_level = 0
    };
    pcnt_channel_handle_t pcnt_chan = NULL;
    pcnt_new_channel(pcnt_unit, &chan_config, &pcnt_chan);
    pcnt_channel_set_edge_action(pcnt_chan, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_channel_set_level_action(pcnt_chan, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_KEEP);
    pcnt_unit_add_watch_point(pcnt_unit, HIGH_LIMIT);
    pcnt_event_callbacks_t cbs = {.on_reach = pcnt_on_overflow,};
    pcnt_unit_register_event_callbacks(pcnt_unit, &cbs, &overflow_counter);
    pcnt_unit_enable(pcnt_unit);
    pcnt_unit_clear_count(pcnt_unit);
    pcnt_unit_start(pcnt_unit);
    xTaskCreate(pcnt_task,"pcntT",2048, NULL, 2, NULL); //if prio is 1 then does hardly get a chance to find a gap in the main loop
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

    vTaskDelay(50);
    init_gpio();
    ledc_init();
    pcnt_init();
    UDPLUS("FPS=%d, FRAMETIME=%d, FRAMES=%d\n",FPS,FRAMETIME,FRAMES);
    vTaskDelay(800); //8s before the movie starts, allows light_level to adjust to environment
    uint64_t start_time,frame_start;
    int frames=0, frame=0;
    while (true) {
        start_time=esp_timer_get_time();
        while ((esp_timer_get_time()-start_time)<10*1000*1000) {
            frame_start=esp_timer_get_time();
            while ((esp_timer_get_time()-frame_start)<FRAMETIME) show_frame_once(frame); //static 7 shows would be more stable
            if (++frame==FRAMES) frame=0;
            frames++;
        }
        UDPLUS("%4.1f frames per second\n",frames/10.0);
        frames=0;
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
