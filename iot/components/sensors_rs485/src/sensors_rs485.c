#include "../include/sensors_rs485.h"

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

static const char *TAG = "RS485";

 

static void echo_send(const int port, const char* str, uint8_t length)
{
    if (uart_write_bytes(port, str, length) != length) {
        ESP_LOGE(TAG, "Send data critical failure.");
        // add your code to handle sending failure here
        abort();
    }
}


//static EventGroupHandle_t Sensors_Event_Handler = NULL;
#define EVENT1 (0x01 << 1)
#define EVENT2 (0x02 << 2)

void rs485_init()
{
     

}

//RS485,获取传感器测量值
void get_sensors_value()
{
    Sensors_Event_Handler = NULL;
    Sensors_Event_Handler = xEventGroupCreate();
    if (Sensors_Event_Handler != NULL)
        printf("创建事假成功\r\n\n");

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
        
            // char *sendData = cJSON_Print(pRoot);
             sendData = cJSON_Print(pRoot);
	         ets_printf("\r\n creatJson : %s\r\n", sendData);

             xEventGroupSetBits(Sensors_Event_Handler,EVENT1);
             vTaskDelay(10000 / portTICK_PERIOD_MS);

             cJSON_free((void *) sendData);                             // 释放cJSON_Print ()分配出来的内存空间
             cJSON_Delete(pRoot);                                       // 释放cJSON_CreateObject ()分配出来的内存空间

        } else {
            // Echo a "." to show we are alive while we wait for input
            ESP_ERROR_CHECK(uart_wait_tx_done(uart_num, 10));
        }
    }
}