/*




*/

#ifndef SYSTEM_H
#define SYSTEM_H

#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <sys/time.h>
#include <driver/adc.h>
#include <driver/gpio.h>
#include "esp_adc_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "ds3231.h"
#include "cJSON.h"

#define SEMAPAHORE_WAIT_MS 5000

#define SDA_GPIO 21
#define SCL_GPIO 22
#define I2C_PORT 0
#define DS3231_ADDR 0x68 //!< I2C address

#define ADC_SAMPLES 5
#define CELL_VOLT_MIN 2.6

#define TIME_FORMAT "%Y-%m-%d %H:%M" // YYYY-MM-DD HH:MM
#define TIME_FORMAT_SEC "%Y-%m-%d %H:%M:%S"
#define TIME_FORMAT_JS "%Y-%m-%dT%H:%M:%S"
#define FILENAME_FORMAT "%Y-%m-%d_%H-%M-%S.csv"
#define TIME_LEN 25

#define VERSION "1.0"

#define SETTINGS_BUFFER 254
#define SETTINGS_PATH "/spiffs/settings.json"

#define WEB_MOUNT_POINT "/spiffs"

#define EXT_LED GPIO_NUM_19
#define INT_LED_GRN GPIO_NUM_26
#define INT_LED_RED GPIO_NUM_25

#define ERROR_MSG_LEN 512 // runtime error flags
#define MEM_OVERFLOW_PERC 80.0


enum error_flag
{
    disk_not_found,
    sensor_not_found,
    coin_cell_low,
    internal_error,
    parsing_error,
    memory_overflow,
    error_flag_num
};

// enum log_level
// {
//     error,
//     warning,
//     info
// };

// internal and external LEDs enable
enum gpio
{
    extr_led,
    intr_led_red,
    intr_led_green
};

class System
{
public:
    static System *instance(void);
    esp_err_t init(void);
    const char *getVersion(void);
    // RTC
    void getTime(tm *time);      // system time
    esp_err_t setTime(tm _time); // system time
    //esp_err_t updateTime(char *timestr);
    esp_err_t getTimeStruct(tm *time, const char *str_time);
    esp_err_t getTimeString(char *buff, size_t len, const char *time_format, const tm time);
    float getTemp(void);
    esp_err_t updateTemp(void);
    // ADC
    float getVoltage(void);
    void updateVoltage(void);
    // LOG
    // esp_err_t log(log_level lvl, const char *msg);
    // esp_err_t getLog(char *buff, size_t len);
    // void getLogStat(char *buff, size_t len);
    esp_err_t setErrorFlag(error_flag flag);
    esp_err_t clearErrorFlag(error_flag flag);
    bool getErrorFlag(error_flag flag);
    esp_err_t checkError(void);
    esp_err_t getErrorMsg(char *buff, size_t len);
    esp_err_t getErrorMsgColor(char *buff, size_t len);
    // GPIO
    esp_err_t setIO(gpio io, bool lvl);
    esp_err_t blink(gpio io, uint8_t times, uint32_t delay);
    // init SPIFFS for settings
    esp_err_t init_spiffs(void);
    bool spiffs_initialized(void);

private:
    static System *inst;
    System();
    System(const System *obj);
    SemaphoreHandle_t xSemaphore = NULL;

    const char *version = VERSION;
    bool _spiffs_initialized = false;
    // RTC
    bool rtc_initialized = false;
    float temp = 0.0;
    float cvolt = 0.0;
    i2c_dev_t rtc;
    esp_err_t init_rtc(void);
    esp_err_t getTimeRTC(tm *_time);
    esp_err_t setTimeRTC(tm _time);
    // ADC
    esp_err_t init_adc(void);
    esp_adc_cal_characteristics_t *adc_chars;
    // LOG
    uint64_t error_register = 0;
    // char log_buff[LOG_BUFFER];
    // log_level log_stat = info;

    // GPIO
    esp_err_t init_gpio(void);
    gpio_num_t extern_led = EXT_LED;
    gpio_num_t intern_led_grn = INT_LED_GRN;
    gpio_num_t intern_led_red = INT_LED_RED;
};

#define SEMAPHORE_TAKE()                                                    \
    do                                                                      \
    {                                                                       \
        if (this->xSemaphore != NULL)                                       \
        {                                                                   \
            if (!xSemaphoreTake(this->xSemaphore, SEMAPAHORE_WAIT_MS / portTICK_RATE_MS))  \
            {                                                               \
                ESP_LOGE(TAG, "SEMAPHORE_TAKE(): failed to take semaphore"); \
                return ESP_ERR_TIMEOUT;                                     \
            }                                                               \
        }                                                                   \
    } while (0)

#define SEMAPHORE_GIVE()                                                    \
    do                                                                      \
    {                                                                       \
        if (this->xSemaphore != NULL)                                       \
        {                                                                   \
            if (!xSemaphoreGive(this->xSemaphore))                          \
            {                                                               \
                ESP_LOGE(TAG, "SEMAPHORE_GIVE(): failed to give semaphore"); \
                return ESP_FAIL;                                            \
            }                                                               \
        }                                                                   \
    } while (0)

#define TIME_DEFAULTS() \
    {                   \
        .tm_sec = 0,    \
        .tm_min = 0,    \
        .tm_hour = 0,   \
        .tm_mday = 0,   \
        .tm_mon = 0,    \
        .tm_year = 0,   \
        .tm_wday = 0,   \
        .tm_yday = 0,   \
        .tm_isdst = 0   \
    }

#endif