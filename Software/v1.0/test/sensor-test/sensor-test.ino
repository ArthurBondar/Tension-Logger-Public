/*
  Connect the SD card to the following pins:

*/
#include "Sensor.h"

Sensor meter;


void printTime(void)
{
  time_t rawTime;
  time( &rawTime );
  struct tm *timeinfo = localtime ( &rawTime );
  Serial.printf( "Current local time and date: %s\n", asctime (timeinfo));
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
  if (rc != 0) Serial.println("setTime failed");
}

void setup()
{
  Serial.begin(115200);
  Serial.println("\n\nSTART");
  printTime();
  setenv("TZ", "UTC-4", 1);
  tzset();
  setTime(6, 12, 2020, 16, 35, 0);
  printTime();


  int ret = meter.init();
  if (ret != SUCCESS) {
    Serial.printf("sensor init: %d\n", ret);
    vTaskDelete( NULL );
  }

}

void loop()
{
  uint8_t ret = meter.readSerial( );
  //vTaskDelay(pdMS_TO_TICKS(10));
  if ( meter.getDataIndex() >= DATA_POINTS ) {
    meter.dumpData();
    meter.clearDataIndex();
  }
  if ( Serial.available() ) {
    char ch = Serial.read();
    meter.setMode( (uint8_t) (ch - '0') );
    Serial.printf("set mode = %d\n", meter.getMode());
  }
}
