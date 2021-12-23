#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
//#include "protocol_examples_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "cJSON.h"

#define HuaWeiYun_report "$oc/devices/615bec4d88056b027dd7ac22_test1234567/sys/properties/report"
esp_mqtt_event_handle_t event;
esp_mqtt_client_handle_t client;

#define ECHO_TASK_STACK_SIZE    (2048)
#define ECHO_TASK_PRIO          (10)

//wifi
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"

//smart_config  and  tcp

#include "../components/smart_config/include/smart_config.h"
#include "../components/sensors_rs485/include/sensors_rs485.h"

#define EXAMPLE_ESP_WIFI_SSID      "360wifi"
#define EXAMPLE_ESP_WIFI_PASS      "1234567890"
#define EXAMPLE_ESP_MAXIMUM_RETRY  10

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
 
static const char *TAG = "MQTT_EXAMPLE";
void start_task(void * pvParameters);
TaskHandle_t StartTask_Handler;

// 与wifi连接、ip获取的有关事件处理
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;

        //IPSTR = ip str    IP2STR = ip to str
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

 //   ESP_ERROR_CHECK(esp_netif_init());
//    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建具有TCP / IP堆栈的默认网络接口实例绑定基站。
    esp_netif_create_default_wifi_sta();               

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //事件句柄
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    
       //esp_event_handler_instance_register(event_base,
                                         //    event_id,
                                         //    event_handler,
                                         //    *event_handler_arg,
                                          //    *instance);
   // event_base类型为：esp_event_base_t；表示 事件基，代表事件的大类（如WiFi事件，IP事件等）
   // event_id类型为：int32_t；表示事件ID，即事件基下的一个具体事件（如WiFi连接丢失，IP成功获取）
   // event_handler类型为：esp_event_handler_t；表示一个handler函数（模板请见第⑤步）
   // *event_handler_arg类型为：void；表示需要传递给handler函数的参数
   // *instance类型为：esp_event_handler_instance_t指针；**[输出]**表示此函数注册的事件实例对象，用于生命周期管理（如删除unrigister这个事件handler）

    //将事件处理程序注册到系统事件循环
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,                            
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    //将事件处理程序注册到系统事件循环                                                    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,

	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */

    //在此阻塞，直至事件组中 WIFI_CONNECTED_BIT | WIFI_FAIL_BIT的标志位其中之一变为1.由阻塞态变为激活态时，把标志位赋值给bits
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
                
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);

    printf("test\n");

    // xTaskCreate(start_task,"start_task",2048,NULL,3,&StartTask_Handler);
    //  vTaskStartScheduler();
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    //printf("Event dispatched from event loop base=%s, event_id=%d \n", base, event_id);
    // 获取MQTT客户端结构体指针
    event = event_data;
    client = event->client;

    // 通过事件ID来分别处理对应的事件
    switch (event->event_id) 
    {
        // 建立连接成功
        case MQTT_EVENT_CONNECTED:
            printf("MQTT_client cnnnect to EMQ ok. \n");          
          //  esp_mqtt_client_publish(client, "domoticz/out", "I am ESP32.", 0, 1, 0);
            // 订阅主题，qos=0
            esp_mqtt_client_subscribe(client, "domoticz/out", 0);      
            break;
        // 客户端断开连接
        case MQTT_EVENT_DISCONNECTED:
            printf("MQTT_client have disconnected. \n");
            break;
        // 主题订阅成功
        case MQTT_EVENT_SUBSCRIBED:
            printf("mqtt subscribe ok. msg_id = %d \n",event->msg_id);
            break;
        // 取消订阅
        case MQTT_EVENT_UNSUBSCRIBED:
            printf("mqtt unsubscribe ok. msg_id = %d \n",event->msg_id);
            break;
        //  主题发布成功
        case MQTT_EVENT_PUBLISHED:
            printf("mqtt published ok. msg_id = %d \n",event->msg_id);
            break;
        // 已收到订阅的主题消息    
        case MQTT_EVENT_DATA:
            printf("mqtt received topic: %.*s \n",event->topic_len, event->topic);
            printf("topic data: %.*s\r\n", event->data_len, event->data);
            break;
        // 客户端遇到错误
        case MQTT_EVENT_ERROR:
            printf("MQTT_EVENT_ERROR \n");
            break;
        default:
            printf("Other event id:%d \n", event->event_id);
            break;
    }
}

static void mqtt_app_start(void * pvParameters)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .host = "a160ff9245.iot-mqtts.cn-north-4.myhuaweicloud.com",
        .username = "615bec4d88056b027dd7ac22_test1234567",
        .password = "8514071e9dc0a07cf11a0e94021a6b15c7279a5f7df9cbae04da599f4785f4e2",
        .client_id = "615bec4d88056b027dd7ac22_test1234567_0_0_2021100612",
        .port = 1883,
        .keepalive = 60,
        .event_handle = mqtt_event_handler 
    };

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
    while(1)
    {
        printf("mqtt\n");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

static void echo_task(void * pvParameters)
{
    get_sensors_value();
}

#define EVENT1 (0x01 << 1)
static void sensor_value_report(void * pvParameters)
{
      EventBits_t sensor_event_bits;
  
    while (1)
    {
        printf("sensor_value_report\n");
            vTaskDelay(2000/portTICK_RATE_MS);
        sensor_event_bits = xEventGroupWaitBits(Sensors_Event_Handler,      //事件的句柄
                            EVENT1,    //感兴趣的事件
                            pdTRUE,             //退出时是否清除事件位
                            pdTRUE,             //是否满足所有事件
                            portMAX_DELAY);     //超时时间，一直等所有事件都满足
         printf("KEY1和KEY2都按下\r\n");
        if(((EVENT1) & sensor_event_bits) == (EVENT1))
           {
              esp_mqtt_client_publish(client, HuaWeiYun_report, sendData, 0, 1, 0);
              printf("aaaaaaaaKEY1和KEY2都按下\r\n");
              
           } 
    }
}


void start_task(void * pvParameters)
{
    
    xTaskCreate(echo_task, "uart_echo_task", 6000, NULL, ECHO_TASK_PRIO, NULL);
    xTaskCreate(sensor_value_report, "sensor_value_report", 6000, NULL, ECHO_TASK_PRIO, NULL);
  
    xTaskCreate(mqtt_app_start, "mqtt_app_start", ECHO_TASK_STACK_SIZE, NULL, ECHO_TASK_PRIO, NULL);
    vTaskDelete(StartTask_Handler);
}


 


 #define EVENT1 (0x01 << 1)
void app_main(void)
{
    // ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());

    //创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());  

     uint8_t mac[6];             //MAC地址缓冲区
    esp_efuse_mac_get_default(mac);
    printf("Default Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);


     

    
     //EventBits_t r_event = pdPASS;
     
   
    if(false)
    smart_config_run();
   
    else{
        //wifi连接
    wifi_init_sta();
    xTaskCreate(start_task,"start_task",2048,NULL,3,&StartTask_Handler);
    vTaskStartScheduler();
    
    
    }
}







