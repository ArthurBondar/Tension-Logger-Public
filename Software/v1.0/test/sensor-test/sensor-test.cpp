/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "Flash.h"
#include "esp_log.h"

#include "Sensor.h"

static const char *TAG = "main";

extern "C" {
    void app_main();
}

void printTime(void)
{
    time_t rawTime;
    time( &rawTime );
    struct tm *timeinfo = localtime ( &rawTime );
    ESP_LOGI(TAG, "Current local time and date: %s", asctime (timeinfo));
}

void setTime(int day, int month, int year, int hour, int min, int sec)
{
    struct tm _now;
    _now.tm_year = year - 1900;
    _now.tm_mon = month - 1;
    _now.tm_mday = day;
    _now.tm_hour = hour;
    _now.tm_min = min;
    _now.tm_sec = sec;

    time_t rawTime = mktime(&_now);
    timeval time_now;
    time_now.tv_sec = rawTime;
    time_now.tv_usec = 0;

    int rc = settimeofday(&time_now, NULL);
    if (rc != 0) ESP_LOGW(TAG, "setTime failed");
}

void app_main(void)
{
    Sensor meter;

    ESP_LOGI(TAG, "START");
    printTime();
    setenv("TZ", "UTC-4", 1);
    tzset();
    setTime(6, 12, 2020, 16, 35, 0);
    printTime();

    int ret = meter.init();
    if (ret != SUCCESS) {
        ESP_LOGE(TAG, "sensor init: %d", ret);
        vTaskDelete( NULL );
    }

    while(1)
    {
        meter.readSerial( );
        //vTaskDelay(pdMS_TO_TICKS(10));
        if ( meter.getDataIndex() >= DATA_POINTS ) {
            meter.dumpData();
            meter.clearDataIndex();
        }
    }

}
