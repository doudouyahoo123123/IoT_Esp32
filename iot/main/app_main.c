#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

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
//#include "queue.h"

#define HuaWeiYun_report "$oc/devices/615bec4d88056b027dd7ac22_test1234567/sys/properties/report"
#define HuaWeiYun_response "$oc/devices/615bec4d88056b027dd7ac22_test1234567/sys/commands/response/request_id="

esp_mqtt_event_handle_t event;
esp_mqtt_client_handle_t client;

#define ECHO_TASK_STACK_SIZE    (2048)
#define ECHO_TASK_PRIO          (10)


//smart_config  and  tcp
#include "../components/smart_config/include/smart_config.h"
#include "../components/sensors_rs485/include/sensors_rs485.h"
#include "../components/wifi/include/wifi.h"

 
static const char *TAG = "MQTT_EXAMPLE";
void start_task(void * pvParameters);
TaskHandle_t StartTask_Handler;

QueueHandle_t Mqtt_command_xQueue;
#define EVENT1 (0x01 << 1)    //rs485
#define EVENT2 (0x02 << 2)    //mqtt command

typedef struct{
    uint8_t data_lenth;
    uint8_t topic_lenth;
    char *data;
    char *topic;
} mqtt_command_queue_t;

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
            // 订阅主题，qos=0
          //  esp_mqtt_client_subscribe(client, "domoticz/out", 0); 
            esp_mqtt_client_subscribe(client, "$oc/devices/615bec4d88056b027dd7ac22_test1234567/sys/commands/# ", 0);       
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
        printf("event->topic_len: %d\r\n",event->topic_len);
        printf("event->data_len: %d\r\n", event->data_len);
        printf("mqtt received topic: %.*s \n",event->topic_len, event->topic);
        printf("topic data: %.*s\r\n", event->data_len, event->data);
        
        mqtt_command_queue_t *command_queue = (mqtt_command_queue_t *)malloc((event->data_len)+(event->topic_len)+2);
        command_queue->data_lenth = event->data_len;
        command_queue->topic_lenth = event->topic_len;
        command_queue->data = event->data;
        command_queue->topic = event->topic;

        int address = (int)command_queue;
        BaseType_t xStatus;
        xStatus = xQueueSendToBack( Mqtt_command_xQueue, (void*)&address, 0 );
        if( xStatus != pdPASS )
		   {
			printf( "Could not send to the queue.\r\n" );
		      }
              else
            xEventGroupSetBits(Sensors_Event_Handler,EVENT2);

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
        printf("mqtt is running\n");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

static void echo_task(void * pvParameters)
{
    get_sensors_value();
}


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

static void Mqtt_command_ReceiverTask(void * pvParameters)
{
    BaseType_t xStatus;
    const TickType_t xTicksToWait = pdMS_TO_TICKS( 100UL );
    int address;
    EventBits_t mqtt_command_event_bits;

    while(true)
	{
        mqtt_command_event_bits = xEventGroupWaitBits(Sensors_Event_Handler,      //事件的句柄
                            EVENT2,    //感兴趣的事件
                            pdTRUE,             //退出时是否清除事件位
                            pdTRUE,             //是否满足所有事件
                            portMAX_DELAY);     //超时时间，一直等所有事件都满足

        if(((EVENT2) & mqtt_command_event_bits) == (EVENT2))
           {
              if( uxQueueMessagesWaiting( Mqtt_command_xQueue ) != 0 )
		{
			printf( "Queue should have been empty!\r\n" );
		}
		
		// 参数1：队列
		// 参数2：接收数据地址
		// 参数3：block 时间
        
		xStatus = xQueueReceive( Mqtt_command_xQueue, (void*)&address, xTicksToWait );
 
		if( xStatus == pdTRUE)
		{
            mqtt_command_queue_t *command_queue;
            command_queue = (mqtt_command_queue_t *)address;
            printf( "Received Topic = %.*s\r\n", command_queue->topic_lenth, command_queue->topic);
			printf( "Received Data=  %.*s\r\n", command_queue->data_lenth, command_queue->data );
            
            char request_id[37];
            sscanf(command_queue->topic,"%*[^=]=%[^{]",request_id);
            printf("request_id = %s\r\n",request_id);
           // char *HuaWeiYun_response_plus_request_id = HuaWeiYun_response;
          //  printf("response = %s\r\n",HuaWeiYun_response_plus_request_id);
            char HuaWeiYun_response_plus_request_id[200];
            //test = HuaWeiYun_response;
            strcpy(HuaWeiYun_response_plus_request_id,HuaWeiYun_response);
            strcat(HuaWeiYun_response_plus_request_id,request_id);

            printf("HuaWeiYun_response_plus_request_id = %s\r\n",HuaWeiYun_response_plus_request_id);
            cJSON *json_parse; 
            char *command_name;
            json_parse = cJSON_Parse(command_queue->data);
           
             if(json_parse)
               {
                   
                command_name = cJSON_GetObjectItem(json_parse, "command_name")->valuestring; //获取name键对应的值的信息
               
                if(!strcmp(command_name,"test_command"))
                {
                     printf("test_command,test_command,test_command,test_command,test_command,test_command \r\n");
                }
                
                if(!strcmp(command_name,"GET_CODE_VERSION"))
                {
                     printf("GET_CODE_VERSION,GET_CODE_VERSION,GET_CODE_VERSION,GET_COD  E_VERSION,GET_CODE_VERSION,GET_CODE_VERSION \r\n");
                      cJSON *pRoot = cJSON_CreateObject();                         // 创建JSON根部结构体
                      cJSON *paras = cJSON_CreateObject();  
                      cJSON_AddNumberToObject(pRoot,"err",0);
                      cJSON_AddStringToObject(pRoot, "err_msg", "none");
                      cJSON_AddItemToObject(pRoot,"paras",paras); 
                      cJSON_AddStringToObject(paras, "version", "V0.0.1");
        
                      char *sendData_response = cJSON_Print(pRoot);
	                  ets_printf("\r\n creatJson : %s\r\n", sendData);

                      esp_mqtt_client_publish(client, HuaWeiYun_response_plus_request_id, sendData_response, 0, 1, 0);

                      cJSON_free((void *) sendData_response);                             // 释放cJSON_Print ()分配出来的内存空间
                      cJSON_Delete(pRoot);                                       // 释放cJSON_CreateObject ()分配出来的内存空间
                }
               }
            cJSON_Delete(json_parse);
            printf("request_id = %s\r\n",request_id);
            free(command_queue);
		}
		else
		{
			printf( "Could not receive from the queue.\r\n" );
		}
              
           } 
		
	}
}


void start_task(void * pvParameters)
{
    xTaskCreate(echo_task, "uart_echo_task", 6000, NULL, ECHO_TASK_PRIO, NULL);
    xTaskCreate(sensor_value_report, "sensor_value_report", 6000, NULL, ECHO_TASK_PRIO, NULL);
    xTaskCreate(mqtt_app_start, "mqtt_app_start", ECHO_TASK_STACK_SIZE, NULL, ECHO_TASK_PRIO, NULL);
    

    if( Mqtt_command_xQueue != NULL )
	{
		xTaskCreate(Mqtt_command_ReceiverTask, "Mqtt_command_ReceiverTask", 6000, NULL, 2, NULL );
	}
    else 
        ESP_LOGW(TAG, "Creat Mqtt command queue failed.");
       
    
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

     //queue:  request_id + data_lenth + data
    Mqtt_command_xQueue = xQueueCreate( 5, sizeof(int) );   //5:depth,300:width

    if(false)
    smart_config_run();
   
    else{
        //wifi连接
    wifi_init_sta();
    xTaskCreate(start_task,"start_task",2048,NULL,3,&StartTask_Handler);
    vTaskStartScheduler();  
    }
}


 




