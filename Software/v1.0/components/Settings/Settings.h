/*




*/

#ifndef SETTINGS_H
#define SETTINGS_H

#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "cJSON.h"

#include "System.h"

#define SETTINGS_BUFFER 254
#define SETTINGS_MAX_VAL 15
#define SETTINGS_MAX_PARAM 15
#define SETTINGS_PATH "/spiffs/settings.json"

// struct settings {
//     tm sys_time;
//     double graph_points;
//     double refresh_rate;
//     double set_point;
//     double mode;
//     bool timestamp;
//     double interval;
//     double file_size;
// };

class Settings
{
public:
    static Settings *instance(void);
    esp_err_t init(void);
    bool getInitialized(void);

    esp_err_t saveToFlash(void);
    esp_err_t getFromFlash(void);

    //esp_err_t setSettingsString(const char* json_str);
    esp_err_t setParameter(const char *param, const char *value);
    esp_err_t setParameter(const char *param, double value);

    esp_err_t getParameter(char *value, size_t len, const char *param);
    esp_err_t getParameter(double *value, const char *param);
    esp_err_t getSettingsString(char *buff, size_t len);

private:
    static Settings *inst;
    Settings();
    Settings(const Settings *obj);
    SemaphoreHandle_t xSemaphore = NULL;

    bool initialized = false;
    char settings_str[SETTINGS_BUFFER] = {0};
    //cJSON *settings_json = NULL;
};

#endif