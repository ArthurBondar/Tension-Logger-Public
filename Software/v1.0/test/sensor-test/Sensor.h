/**************************************************************************/
/*!
  @file     Sensor.h

  Wrapper Class for communcating with SD card throught SD MMC interface

*/
/**************************************************************************/

#ifndef _SENSOR_H_
#define _SENSOR_H_

#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HardwareSerial.h"
// extern "C" {
// #include "driver/uart.h"
// #include "driver/gpio.h"
// #include "sdkconfig.h"
// //#include "esp32-hal-log.h"
// //#include "esp_err.h"
// };

//#include "esp_log.h"  // arduino func

#define SUCCESS 0
#define FAILED 1
#define NO_SER_DATA 2

#define RXD2 16
#define TXD2 17

#define DATA_POINTS 5
#define LINE_BUFF   100
#define SERIAL_BUFF 128
#define UNITS_LEN 5


struct SensorData
{
    struct tm timestamp;
    float tension;
    char unit1[UNITS_LEN];
    float peak_tension;
    char unit2[UNITS_LEN];
};


class Sensor
{
public:
    uint8_t init (unsigned long baud = 9600, bool date_en = true);
    uint8_t readSerial ( void );
    char* getRawData (void);
    void resetLine(void);
    void setMode(uint8_t mode);
    uint8_t getMode(void);
    uint8_t deinit (void);
    uint8_t setBaud (unsigned long baud);
    uint8_t getData (SensorData *data, size_t index);
    uint8_t getDataIndex(void);
    void clearDataIndex(void);
    void dumpData( void );

private:
    bool _date_en = true;
    bool _initialized = false;
    uint8_t _mode = 1;
    char _line[LINE_BUFF];
    size_t _line_i = 0;
    SensorData _data[DATA_POINTS];
    size_t _data_i = 0;
    uint8_t _parseData();

};


#endif // Sensor.h