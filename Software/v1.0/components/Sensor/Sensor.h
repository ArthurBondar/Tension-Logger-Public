/**************************************************************************/
/*!
  @file     Sensor.h

  Wrapper Class for communcating with SD card throught SD MMC interface

*/
/**************************************************************************/

#ifndef SENSOR_H
#define SENSOR_H

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/uart.h"
#include "driver/gpio.h"

#include "esp_log.h"
#include "esp_err.h"

#include "System.h"

// extern "C" {
// #include "driver/uart.h"
// #include "driver/gpio.h"
// #include "sdkconfig.h"
// //#include "esp32-hal-log.h"

// };

//#include "esp_log.h"  // arduino func

#define RXD2 16
#define TXD2 17

#define SERIAL_BUFF 256
#define UNITS_LEN 5
#define DEFAULT_BAUD 9600

#define SENSOR_DEFAULTS()                                             \
    {                                                                 \
        .timestamp = {                                                \
            .tm_sec = 0,                                              \
            .tm_min = 0,                                              \
            .tm_hour = 0,                                             \
            .tm_mday = 0,                                             \
            .tm_mon = 0,                                              \
            .tm_year = 0,                                             \
            .tm_wday = 0,                                             \
            .tm_yday = 0,                                             \
            .tm_isdst = 0},                                           \
        .tension = 0.0, .peak_tension = -1.0, .units = { 0 } \
    }

struct SensorData
{
    tm timestamp;
    float tension;
    float peak_tension;
    char units[UNITS_LEN];
};

class Sensor
{
public:
    static Sensor *instance(void);
    esp_err_t init(void);
    esp_err_t readSerial(uint32_t delay);
    esp_err_t setBaud(uint32_t baud);
    esp_err_t getData(SensorData* data_buff);
    void deinit(void);
    void dumpData(SensorData *data, int len);
    void flush(void);
    // esp_err_t setMode(uint8_t mode);
    // uint8_t getMode(void);

private:
    static Sensor *inst;
    Sensor();
    SemaphoreHandle_t xSemaphore = NULL;
    bool _initialized = false;
    uint8_t _mode = 1;
    SensorData _data = SENSOR_DEFAULTS();

};

#endif // Sensor.h