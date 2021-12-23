#include "../include/smart_config.h"
//#include "esp_wpa2.h"
//#include "nvs_flash.h"


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

void smart_config_run()
{
    printf("hello doudou\n");
    initialise_wifi();
}

