/*




*/

#include "Settings.h"

static const char *TAG = "Settings";

/* Null, because instance will be initialized on demand. */
Settings *Settings::inst = 0;

//
Settings::Settings()
{
    // Semaphore cannot be used before a call to vSemaphoreCreateBinary ().
    // This is a macro so pass the variable in directly.
    this->xSemaphore = xSemaphoreCreateMutex();
    if (this->xSemaphore == NULL)
        ESP_LOGE(TAG, "Settings(): failed to create semaphore");
}

//
Settings *Settings::instance(void)
{
    if (inst == 0)
    {
        ESP_LOGI(TAG, "creating Settings instance");
        inst = new Settings();
    }
    return inst;
}

// inialize the RTC, GPIOs and ADC
esp_err_t Settings::init(void)
{
    if (!System::instance()->spiffs_initialized())
    {
        if (System::instance()->init_spiffs() != ESP_OK)
            return ESP_FAIL;
    }
    if (this->getFromFlash() != ESP_OK)
        return ESP_FAIL;

    this->initialized = true;
    return ESP_OK;
}

// save internal settings string to flash
esp_err_t Settings::saveToFlash(void)
{
    SEMAPHORE_TAKE();
    // write
    FILE *fptr = fopen(SETTINGS_PATH, "w");
    if (fptr == NULL)
    {
        ESP_LOGE(TAG, "saveToFlash(): failed to open settings file");
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    if (fprintf(fptr, "%s", this->settings_str) < 0)
        ESP_LOGW(TAG, "saveToFlash(): error in fprintf function");
    fclose(fptr);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

// recover internal settings string from flash
esp_err_t Settings::getFromFlash(void)
{
    SEMAPHORE_TAKE();
    FILE *fptr = fopen(SETTINGS_PATH, "r");
    if (fptr == NULL)
    {
        ESP_LOGE(TAG, "getFromFlash(): failed to open %s", SETTINGS_PATH);
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    memset(settings_str, 0, SETTINGS_BUFFER);
    size_t read = fread(settings_str, 1, SETTINGS_BUFFER, fptr);
    if (read <= 0)
        ESP_LOGW(TAG, "getFromFlash(): fread failed");
    fclose(fptr);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

// set settings @param to @value in internal memory (string)
esp_err_t Settings::setParameter(const char *param, const char *value)
{
    SEMAPHORE_TAKE();
    cJSON *root = cJSON_Parse(this->settings_str);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "setParameter(): invalid json");
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    cJSON *value_obj = cJSON_CreateString(value);
    cJSON *param_obj = cJSON_GetObjectItem(root, param);
    if (param_obj == NULL)
        cJSON_AddItemToObject(root, param, value_obj);
    else
        cJSON_ReplaceItemInObject(root, param, value_obj);

    cJSON_bool success = cJSON_PrintPreallocated(root, this->settings_str, SETTINGS_BUFFER, true);
    cJSON_Delete(root);
    SEMAPHORE_GIVE();
    if (!success)
        return ESP_FAIL;
    return ESP_OK;
}

// set settings @param to @value in internal memory (double)
esp_err_t Settings::setParameter(const char *param, double value)
{
    SEMAPHORE_TAKE();
    cJSON *root = cJSON_Parse(this->settings_str);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "setParameter(): invalid json");
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    cJSON *value_obj = cJSON_CreateNumber(value);
    cJSON *param_obj = cJSON_GetObjectItem(root, param);
    if (param_obj == NULL)
        cJSON_AddItemToObject(root, param, value_obj);
    else
        cJSON_ReplaceItemInObject(root, param, value_obj);

    cJSON_bool success = cJSON_PrintPreallocated(root, this->settings_str, SETTINGS_BUFFER, true);
    cJSON_Delete(root);
    SEMAPHORE_GIVE();
    if (!success)
        return ESP_FAIL;
    return ESP_OK;
}

// get settings @param into @value (string) ESP_FAIL if doesnt exist
esp_err_t Settings::getParameter(char *value, size_t len, const char *param)
{
    if ((len < SETTINGS_MAX_VAL) || value == NULL)
        return ESP_FAIL;

    SEMAPHORE_TAKE();
    cJSON *root = cJSON_Parse(this->settings_str);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "getParameter(): invalid json");
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    cJSON *value_obj = cJSON_GetObjectItem(root, param);
    if (value_obj == NULL)
    {
        ESP_LOGE(TAG, "getParameter(): settings[%s] doesnt exist", param);
        cJSON_Delete(root);
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    if (!cJSON_IsString(value_obj))
    {
        cJSON_Delete(root);
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    strncpy(value, value_obj->valuestring, len);
    cJSON_Delete(root);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

// get settings @param into @value (double) ESP_FAIL if doesnt exist
esp_err_t Settings::getParameter(double *value, const char *param)
{
    SEMAPHORE_TAKE();
    cJSON *root = cJSON_Parse(this->settings_str);
    if (root == NULL)
    {
        ESP_LOGE(TAG, "getParameter(): invalid json");
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    cJSON *value_obj = cJSON_GetObjectItem(root, param);
    if (value_obj == NULL)
    {
        ESP_LOGE(TAG, "getParameter(): settings[%s] doesnt exist", param);
        cJSON_Delete(root);
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    if (!cJSON_IsNumber(value_obj))
    {
        cJSON_Delete(root);
        SEMAPHORE_GIVE();
        return ESP_FAIL;
    }
    *value = value_obj->valuedouble;
    cJSON_Delete(root);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

// get settings @param into @value (string) ESP_FAIL if doesnt exist
esp_err_t Settings::getSettingsString(char *buff, size_t len)
{
    if ((len < SETTINGS_BUFFER) || buff == NULL)
        return ESP_FAIL;

    SEMAPHORE_TAKE();
    strncpy(buff, this->settings_str, SETTINGS_BUFFER);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

// get initialzied flag
bool Settings::getInitialized(void)
{
    return this->initialized;
}

// set internal settings string to @json_str and save to falsh
// esp_err_t Settings::setSettingsString(const char *json_str)
// {
//     cJSON *root = cJSON_Parse(json_str);
//     if (root == NULL)
//     {
//         ESP_LOGE(TAG, "setSettingsString(): invalid json");
//         return ESP_FAIL;
//     }
//     cJSON_Delete(root);
//     SEMAPHORE_TAKE();
//     memset(this->settings_str, 0, SETTINGS_BUFFER);
//     strncpy(this->settings_str, json_str, SETTINGS_BUFFER);
//     SEMAPHORE_GIVE();
//     if (this->saveToFlash() != ESP_OK)
//         return ESP_FAIL;
//     return ESP_OK;
// }
