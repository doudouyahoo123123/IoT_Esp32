#ifndef _SENSORS_RS485_H_
#define _SENSORS_RS485_H_

//rs485
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "/home/doudou/esp/esp32/esp-idf/components/json/cJSON/cJSON.h"
#include "freertos/event_groups.h"


void get_sensors_value();
EventGroupHandle_t Sensors_Event_Handler;
char *sendData;

#endif
