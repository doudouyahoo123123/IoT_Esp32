/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
//mqtt
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
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

//rs485
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"

//#define TAG "RS485_ECHO_APP"
#define ECHO_TEST_TXD   (23)
#define ECHO_TEST_RXD   (22)
#define ECHO_TEST_RTS   (18)
//#define ECHO_TEST_RTS   (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS   (UART_PIN_NO_CHANGE)
#define BUF_SIZE        (127)
#define BAUD_RATE       (9600)
#define PACKET_READ_TICS        (100 / portTICK_RATE_MS)
#define ECHO_TASK_STACK_SIZE    (2048)
#define ECHO_TASK_PRIO          (10)
#define ECHO_UART_PORT          (2)
#define ECHO_READ_TOUT          (3) // 3.5T * 8 = 28 ticks, TOUT=3 -> ~24..33 ticks

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
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"


#include <sys/param.h>
#include "tcpip_adapter.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#define EXAMPLE_ESP_WIFI_SSID      "360wifi"
#define EXAMPLE_ESP_WIFI_PASS      "1234567890"
#define EXAMPLE_ESP_MAXIMUM_RETRY  10

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;
 
static const char *TAG = "MQTT_EXAMPLE";

// An example of echo test with hardware flow control on UART
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
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
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
}

static void echo_send(const int port, const char* str, uint8_t length)
{
    if (uart_write_bytes(port, str, length) != length) {
        ESP_LOGE(TAG, "Send data critical failure.");
        // add your code to handle sending failure here
        abort();
    }
}

static void echo_task(void * pvParameters)
{
     const int uart_num = ECHO_UART_PORT;
    uart_config_t uart_config = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_APB,
    };

    // Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Start RS485 application test and configure UART.");

    // Install UART driver (we don't need an event queue here)
    // In this example we don't even use a buffer for sending data.
    ESP_ERROR_CHECK(uart_driver_install(uart_num, BUF_SIZE * 2, 0, 0, NULL, 0));

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    ESP_LOGI(TAG, "UART set pins, mode and install driver.");

    // Set UART pins as per KConfig settings
    ESP_ERROR_CHECK(uart_set_pin(uart_num, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(uart_num,UART_MODE_RS485_HALF_DUPLEX));

    // Set read timeout of UART TOUT feature
    ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, ECHO_READ_TOUT));

    // Allocate buffers for UART
    uint8_t* data = (uint8_t*) malloc(BUF_SIZE);

    ESP_LOGI(TAG, "UART start recieve loop.\r\n");


    while(1) {
        //Read data from UART
        char check_data[8]={0x01,0x03,0x00,0x00,0x00,0x02,0xC4,0x0B};

        printf("check_data send:%s\n",check_data);
        echo_send(uart_num, check_data, 8);

        int len = uart_read_bytes(uart_num, data, BUF_SIZE, PACKET_READ_TICS);
        printf("len:%d\n",len);
        //Write data back to UART
        if (len > 0) {
        
            float temperature = 0;
            float huminity = 0;
            if((uint8_t)data[0] == 0x01 && (uint8_t)data[1] == 0x03)
            {
                uint16_t temp_temperature_16 = ( (uint8_t)data[5]<<8) | (uint8_t)data[6];
                uint16_t temperature_pre = temp_temperature_16/10;
                uint16_t temperature_latter = temp_temperature_16%10;
                temperature = (float)temperature_pre + (float)temperature_latter*(0.1);

                uint16_t temp_huminity_16 = ( (uint8_t)data[3]<<8) | (uint8_t)data[4];
                uint16_t huminity_pre = temp_huminity_16/10;
                uint16_t huminity_latter = temp_huminity_16%10;
                huminity = (float)huminity_pre + (float)huminity_latter*(0.1);

                printf("temperature:%f\n",temperature);
                printf("huminity:%f\n",huminity);
              // ro   
            }

            char prefix[] = "RS485 Received: [";
            ESP_LOGI(TAG, "Received %u bytes:", len);
            printf("[ ");
            for (int i = 0; i < len; i++) {
                printf("0x%.2X ", (uint8_t)data[i]);
            }
            printf("] \n");
            
              
             cJSON *pRoot = cJSON_CreateObject();                         // 创建JSON根部结构体
             cJSON * pArray = cJSON_CreateArray();                        // 创建数组类型结构体
             cJSON_AddItemToObject(pRoot,"services",pArray);                  // 添加数组到根部结构体
             cJSON * pArray_relay = cJSON_CreateObject();                 // 创建JSON子叶结构体
             cJSON_AddItemToArray(pArray,pArray_relay);                   // 添加子叶结构体到数组结构体            
             cJSON_AddStringToObject(pArray_relay, "service_id", "gateway");        // 添加字符串类型数据到子叶结构体
             cJSON *pValue = cJSON_CreateObject();                        // 创建JSON子叶结构体
             cJSON_AddItemToObject(pArray_relay, "properties", pValue);        // 添加字符串类型数据到子叶结构体
             cJSON_AddNumberToObject(pValue,"temperature",temperature);
             cJSON_AddNumberToObject(pValue,"huminity",huminity);
        
             char *sendData = cJSON_Print(pRoot);
	         ets_printf("\r\n creatJson : %s\r\n", sendData);

             esp_mqtt_client_publish(client, HuaWeiYun_report, sendData, 0, 1, 0);  

             cJSON_free((void *) sendData);                             // 释放cJSON_Print ()分配出来的内存空间
             cJSON_Delete(pRoot);                                       // 释放cJSON_CreateObject ()分配出来的内存空间

        } else {
            // Echo a "." to show we are alive while we wait for input
            ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 10));
        }
       vTaskDelay(5000 / portTICK_PERIOD_MS);    
    }
    vTaskDelete(NULL);
}


static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
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

TaskHandle_t StartTask_Handler;
void start_task(void * pvParameters)
{
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, ECHO_TASK_PRIO, NULL);
    xTaskCreate(mqtt_app_start, "mqtt_app_start", ECHO_TASK_STACK_SIZE, NULL, ECHO_TASK_PRIO, NULL);
    vTaskDelete(StartTask_Handler);
}




















/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG1 = "smartconfig_example";

static void tcp_client_task(void *pvParameters)
{
    int sockfd = 0;
        /* 打开一个socket套接字 */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);


    if (-1 == sockfd)
	{
		printf("socket open failure !!! \n");
		//return -1;
	}
    else
    {
        int ret = 0;
        struct sockaddr_in seraddr = {0};
        seraddr.sin_family = AF_INET;		                    // 设置地址族为IPv4
	    seraddr.sin_port = htons(6666);	                    // 设置地址的端口号信息
	    seraddr.sin_addr.s_addr = inet_addr("192.168.1.109");	//　设置IP地址
	     ret = connect(sockfd, (const struct sockaddr *)&seraddr, sizeof(seraddr));
        if(ret < 0)
            printf("connect to server failure !!! \n");
        else
        {
            printf("connect success, ret = %d.\n", ret);
        }          
    }
    int ret_1 = 0;
                        int cnt = 100;
	while(cnt--)
	{
	    ret_1 = send(sockfd, "I am ESP32.", strlen("I am ESP32."), 0);
	    if(ret_1 < 0)
	        printf("send err. \n");
	    else
	        printf("send ok. \n");
	    vTaskDelay(2000 / portTICK_PERIOD_MS);   /* 延时2000ms*/
	}  
    close(sockfd);
    vTaskDelete(NULL);
}

static void smartconfig_example_task(void * parm);

static void event_handler1(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG1, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG1, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG1, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = { 0 };
        uint8_t password[65] = { 0 };
        uint8_t rvd_data[33] = { 0 };

        uint8_t phone_ip[4] = {0};
       // uint8_t bssid[6] = {0};
       // uint8_t token = 0;

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true) {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        memcpy(phone_ip,evt->cellphone_ip,sizeof(evt->cellphone_ip));

        ESP_LOGI(TAG1, "SSID:%s", ssid);
        ESP_LOGI(TAG1, "PASSWORD:%s", password);
        ESP_LOGI(TAG1, "CELLPHONE_IP:%s",phone_ip);
        ESP_LOGI(TAG1, "CELLPHONE_IP:%x",phone_ip[0]);
        ESP_LOGI(TAG1, "CELLPHONE_IP:%x",phone_ip[1]);
        ESP_LOGI(TAG1, "CELLPHONE_IP:%x",phone_ip[2]);
        ESP_LOGI(TAG1, "CELLPHONE_IP:%x",phone_ip[3]);

        if (evt->type == SC_TYPE_ESPTOUCH_V2) {
            ESP_ERROR_CHECK( esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)) );
            ESP_LOGI(TAG1, "RVD_DATA:");
            for (int i=0; i<33; i++) {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect(); 
       
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

static void initialise_wifi(void)
{
 //   ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
 //   ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler1, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler1, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler1, NULL) );

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

static void smartconfig_example_task(void * parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK( esp_smartconfig_set_type(SC_TYPE_ESPTOUCH) );
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_smartconfig_start(&cfg) );
    while (1) {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if(uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG1, "WiFi Connected to ap");
        }
        if(uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG1, "smartconfig over");
            esp_smartconfig_stop();
            xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
            vTaskDelete(NULL);
        }
    }
}




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
    ESP_ERROR_CHECK(esp_event_loop_create_default());

     uint8_t mac[6];             //MAC地址缓冲区
    esp_efuse_mac_get_default(mac);
    printf("Default Mac Address = %02X:%02X:%02X:%02X:%02X:%02X\r\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
   
    if(false)
    initialise_wifi();
    else{
    wifi_init_sta();

    xTaskCreate(start_task,"start_task",2048,NULL,3,&StartTask_Handler);
    vTaskStartScheduler();
    }
}







