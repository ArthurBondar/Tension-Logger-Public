/*




*/

#include "System.h"

static const char *TAG = "System";
// static const char *disk_error_msg = "  storage card not found";   //23
// static const char *sensor_error_msg = "  sensor not connected";   // 20
// static const char *coincell_error_msg = "  coincell battery low"; //20
// static const char *internall_error_msg = "  internall error";     //15
static const char *no_errors = "normal operation";

static const char *errors[error_flag_num] = {
    "storage card not found",
    "sensor not connected",
    "coincell battery low",
    "internal error",
    "sensor parsing error",
    "memory overflow"};

/* Null, because instance will be initialized on demand. */
System *System::inst = 0;

//
System::System()
{
    // Semaphore cannot be used before a call to vSemaphoreCreateBinary ().
    // This is a macro so pass the variable in directly.
    this->xSemaphore = xSemaphoreCreateMutex();
    if (this->xSemaphore == NULL)
        ESP_LOGE(TAG, "System(): failed to create semaphore");
}

//
System *System::instance(void)
{
    if (inst == 0)
    {
        ESP_LOGI(TAG, "creating System instance");
        inst = new System();
    }
    return inst;
}

// inialize the RTC, GPIOs and ADC
esp_err_t System::init(void)
{
    tm time;

    // initialising the RTC
    if (this->init_rtc() != ESP_OK)
        return ESP_FAIL;
    if (this->getTimeRTC(&time) != ESP_OK)
        return ESP_FAIL;
    if (this->setTime(time) != ESP_OK)
        return ESP_FAIL;
    if (this->updateTemp() != ESP_OK)
        ESP_LOGW(TAG, "init(): failed to get temp");
    // this->getTime(&time);
    // char time_buff[TIME_LEN];
    // this->getTimeString(time_buff, sizeof(time_buff), time);
    //ESP_LOGI(TAG, "time from system %s, temp = %.2f C", time_buff, this->getTemp());
    // init ADC
    if (this->init_adc() != ESP_OK)
        return ESP_FAIL;
    this->updateVoltage();
    if (this->getVoltage() < CELL_VOLT_MIN)
        this->setErrorFlag(coin_cell_low);
    // init GPIO
    if (this->init_gpio() != ESP_OK)
        return ESP_FAIL;
    // init spiffs
    if (this->init_spiffs() != ESP_OK)
        return ESP_FAIL;

    return ESP_OK;
}

// get software version
const char *System::getVersion(void)
{
    return this->version;
}

// initialize the RTC - called by init()
esp_err_t System::init_rtc(void)
{
    // initialising the RTC
    esp_err_t err = i2cdev_init();
    if (err != ESP_OK)
    {
        // TODO: handle error
        return ESP_FAIL;
    }

    // init i2c peripheral
    memset(&this->rtc, 0, sizeof(i2c_dev_t));
    this->rtc.port = I2C_PORT;
    this->rtc.addr = DS3231_ADDR;
    this->rtc.cfg.sda_io_num = SDA_GPIO;
    this->rtc.cfg.scl_io_num = SCL_GPIO;
    this->rtc.cfg.master.clk_speed = 400000;

    if (i2c_dev_create_mutex(&this->rtc) != ESP_OK)
    {
        ESP_LOGE(TAG, "Mutex not taken");
        return ESP_FAIL;
    }
    this->rtc_initialized = true;
    return ESP_OK;
}

// get value of the temp buffer in memory
float System::getTemp(void)
{
    return this->temp;
}

//  update temp parameter from the ds3231 chip
esp_err_t System::updateTemp(void)
{
    if (this->rtc_initialized)
        return ds3231_get_temp_float(&this->rtc, &this->temp);
    else
        return ESP_FAIL;
}

//  get system time
void System::getTime(tm *_time)
{
    time_t raw_time = {0};
    time(&raw_time);
    localtime_r(&raw_time, _time);
}

//
esp_err_t System::setTime(tm _time)
{
    time_t s_epoc = mktime(&_time);
    // struct timeval { time_t      tv_sec; suseconds_t tv_usec;
    timeval us_epoc;
    us_epoc.tv_sec = s_epoc;
    us_epoc.tv_usec = 0;
    // set system time
    if (settimeofday(&us_epoc, NULL) != 0)
        return ESP_FAIL;
    // set rtc time
    if (this->setTimeRTC(_time) != ESP_OK)
        return ESP_FAIL;
    return ESP_OK;
}

//
esp_err_t System::getTimeRTC(tm *_time)
{
    if (this->rtc_initialized)
    {
        if (ds3231_get_time(&this->rtc, _time) != ESP_OK)
        {
            // TODO: handle error
            ESP_LOGE(TAG, "getting time from RTC failed");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

//
esp_err_t System::setTimeRTC(tm _time)
{
    esp_err_t err = ESP_FAIL;
    if (this->rtc_initialized)
    {
        err = ds3231_set_time(&this->rtc, &_time);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "setTimeRTC(): failed");
    }
    return err;
}

//
esp_err_t System::getTimeString(char *buff, size_t len, const char *time_format, const tm time)
{
    if (len < TIME_LEN)
        return ESP_ERR_INVALID_ARG;
    strftime(buff, len, time_format, &time);
    return ESP_OK;
}

//
esp_err_t System::getTimeStruct(tm *time, const char *str_time)
{
    if (time == NULL)
    {
        ESP_LOGE(TAG, "getTimeStruct(): time == NULL");
        return ESP_FAIL;
    }
    strptime(str_time, TIME_FORMAT, time);
    time->tm_sec = 0;
    return ESP_OK;
}

//
esp_err_t System::init_adc(void)
{
    esp_err_t ret = ESP_OK;
    ret = adc1_config_width(ADC_WIDTH_BIT_12);
    ret |= adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_11db);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "init_adc(): failed");
        return ESP_FAIL;
    }

    //Characterize ADC at particular atten
    this->adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, 1100, this->adc_chars);
    //esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_11db, ADC_WIDTH_BIT_12, 1100, this->adc_chars);
    // if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    //     printf("eFuse Vref");
    // } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
    //     printf("Two Point");
    // } else {
    //     printf("Default");
    // }
    return ESP_OK;
}

//
void System::updateVoltage(void)
{
    uint32_t adc_reading = 0;
    for (int i = 0; i < ADC_SAMPLES; i++)
        adc_reading += adc1_get_raw(ADC1_CHANNEL_7);
    adc_reading /= ADC_SAMPLES;
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, this->adc_chars);
    this->cvolt = voltage / 1000.0;
    //ESP_LOGI(TAG, "Raw: %d\tVoltage: %dmV .3%f\n", adc_reading, voltage, vol);
}

//
float System::getVoltage(void)
{
    return this->cvolt;
}

//
esp_err_t System::init_gpio(void)
{
    gpio_config_t io_conf;
    // configure external Green led pin
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pin_bit_mask = 1ULL << (uint32_t)EXT_LED; // bit mask: GPIO_SD_CD (27)
    if (gpio_config(&io_conf) != ESP_OK)
    {
        //ESP_LOGE(TAG, "init(): CD pin configure failed");
        return ESP_FAIL;
    }
    // configure internal Green led pin
    io_conf.pin_bit_mask = 1ULL << (uint32_t)INT_LED_GRN; // bit mask: GPIO_SD_CD (27)
    if (gpio_config(&io_conf) != ESP_OK)
    {
        //ESP_LOGE(TAG, "init(): CD pin configure failed");
        return ESP_FAIL;
    }
    // configure external Red led pin
    io_conf.pin_bit_mask = 1ULL << (uint32_t)INT_LED_RED; // bit mask: GPIO_SD_CD (27)
    if (gpio_config(&io_conf) != ESP_OK)
    {
        //ESP_LOGE(TAG, "init(): CD pin configure failed");
        return ESP_FAIL;
    }

    return ESP_OK;
}

//
esp_err_t System::setIO(gpio io, bool lvl)
{
    SEMAPHORE_TAKE();
    if (io == extr_led)
    {
        gpio_set_level(EXT_LED, lvl);
    }
    else if (io == intr_led_green)
    {
        gpio_set_level(INT_LED_GRN, lvl);
    }
    else if (io == intr_led_red)
    {
        gpio_set_level(INT_LED_RED, lvl);
    }
    SEMAPHORE_GIVE();
    return ESP_OK;
}

//
esp_err_t System::blink(gpio io, uint8_t times, uint32_t delay)
{
    SEMAPHORE_TAKE();
    bool state = true;
    for (uint8_t i = 0; i < times; i++)
    {
        if (io == extr_led)
            gpio_set_level(EXT_LED, state);
        else if (io == intr_led_green)
            gpio_set_level(INT_LED_GRN, state);
        else if (io == intr_led_red)
            gpio_set_level(INT_LED_RED, state);
        state = (!state) ? true : false;
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
    SEMAPHORE_GIVE();
    return ESP_OK;
}

//
esp_err_t System::setErrorFlag(error_flag flag)
{
    uint64_t mask = 0x1;
    SEMAPHORE_TAKE();
    this->error_register |= (mask << flag);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

//
esp_err_t System::clearErrorFlag(error_flag flag)
{
    uint64_t mask = 0x1;
    SEMAPHORE_TAKE();
    this->error_register &= ~(mask << flag);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

//
bool System::getErrorFlag(error_flag flag)
{
    uint64_t mask = 0x1;
    SEMAPHORE_TAKE();
    bool rc = this->error_register & (mask << flag);
    SEMAPHORE_GIVE();
    return rc;
}

//
esp_err_t System::checkError(void)
{
    esp_err_t rc;
    SEMAPHORE_TAKE();
    if (this->error_register == 0)
        rc = ESP_OK;
    else
        rc = ESP_FAIL;
    SEMAPHORE_GIVE();
    return rc;
}

// print comma separated error message into a buffer
esp_err_t System::getErrorMsg(char *buff, size_t len)
{
    if (buff == NULL || len < ERROR_MSG_LEN)
        return ESP_FAIL;
    memset(buff, 0, len);
    uint64_t mask = 0x1;
    SEMAPHORE_TAKE();
    if (this->error_register == 0)
    {
        snprintf(buff, ERROR_MSG_LEN, "%s,", no_errors);
    }
    else
    {
        for (int i = 0; i < error_flag_num; i++)
        {
            if (this->error_register & mask)
            {
                strncat(buff, errors[i], ERROR_MSG_LEN);
                strncat(buff, ",", ERROR_MSG_LEN);
            }
            mask <<= 1;
        }
    }
    // // internall error
    // if (this->error_register & (mask << internal_error))
    //     strncat(buff, internall_error_msg, len);
    // // disk error
    // if (this->error_register & (mask << disk_not_found))
    //     strncat(buff, disk_error_msg, len);
    // // sensor error
    // if (this->error_register & (mask << sensor_not_found))
    //     strncat(buff, sensor_error_msg, len);
    // // coin_cell_low error
    // if (this->error_register & (mask << coin_cell_low))
    //     strncat(buff, coincell_error_msg, len);
    // // no errors
    // if (this->error_register == 0)
    //     strncat(buff, no_errors, len);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

//
esp_err_t System::getErrorMsgColor(char *buff, size_t len)
{
    if (buff == NULL || len < 9)
        return ESP_FAIL;
    memset(buff, 0, len);
    SEMAPHORE_TAKE();

    if (this->error_register == 0)
        strncpy(buff, "success", len);
    else
        strncpy(buff, "warning", len);
    SEMAPHORE_GIVE();
    return ESP_OK;
}

// initialized SPI-FS for web serving content
esp_err_t System::init_spiffs()
{
    if (this->_spiffs_initialized == true)
        return ESP_FAIL;

    esp_vfs_spiffs_conf_t conf = {
        .base_path = WEB_MOUNT_POINT,
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = false};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK)
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
    else
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d (%d%%)", total, used, (used * 100) / total);

    this->_spiffs_initialized = true;
    return ESP_OK;
}

//
bool System::spiffs_initialized(void)
{
    return this->_spiffs_initialized;
}

//
// void System::printTimeStruct(tm time_struct)
// {
//     ESP_LOGI(TAG,"printTimeStruct(): %d-%02d-%02d %02d:%02d:%02d",
//                  time_struct.tm_year + 1900,
//                  time_struct.tm_mon + 1,
//                  time_struct.tm_mday,
//                  time_struct.tm_hour,
//                  time_struct.tm_min,
//                  time_struct.tm_sec);

//     // ESP_LOGI(TAG,"printTimeStruct(): %d-%02d-%02d %02d:%02d:%02d",
//     //             time_struct.tm_year,
//     //             time_struct.tm_mon,
//     //             time_struct.tm_mday,
//     //             time_struct.tm_hour,
//     //             time_struct.tm_min,
//     //             time_struct.tm_sec);
// }

//
// void System::printSettingsStruct(settings set_struct)
// {
//     ESP_LOGI(TAG, "graph_points: %.2f", set_struct.graph_points);
//     ESP_LOGI(TAG, "refresh_rate: %.2f", set_struct.refresh_rate);
//     ESP_LOGI(TAG, "set_point: %.2f", set_struct.set_point);
//     ESP_LOGI(TAG, "mode: %.2f", set_struct.mode);
//     ESP_LOGI(TAG, "timestamp: %s", (set_struct.timestamp == true) ? "true" : "false");
//     ESP_LOGI(TAG, "interval: %.2f", set_struct.interval);
//     ESP_LOGI(TAG, "file_size: %.2f", set_struct.file_size);
// }

//
// esp_err_t System::log(log_level lvl, const char *msg)
// {
//     SEMAPHORE_TAKE();
//     snprintf(this->log_buff, sizeof(this->log_buff), "%s", msg);
//     this->log_stat = lvl;
//     SEMAPHORE_GIVE();
//     return ESP_OK;
// }

// //
// esp_err_t System::getLog(char *buff, size_t len)
// {
//     SEMAPHORE_TAKE();
//     snprintf(buff, len, "%s", this->log_buff);
//     SEMAPHORE_GIVE();
//     return ESP_OK;
// }

// //
// void System::getLogStat(char *buff, size_t len)
// {
//     if (this->log_stat == info)
//         strncpy(buff, "success", len);
//     if (this->log_stat == warning)
//         strncpy(buff, "warning", len);
//     if (this->log_stat == error)
//         strncpy(buff, "danger", len);
// }

// get settings
// settings System::getSettings(void)
// {
//     return this->_set;
// }

// //
// esp_err_t System::getSettingsStr(char *buff, size_t len, settings set)
// {
//     cJSON *root = NULL;

//     root = cJSON_CreateObject();
//     if (root == NULL) {
//         //ESP_LOGE(TAG, "getSettingsStr(): failed to allocate memory for cJSON object");
//         return ESP_FAIL;
//     }
//     // display field
//     cJSON_AddNumberToObject(root, "graph_points", set.graph_points);
//     cJSON_AddNumberToObject(root, "refresh_rate", set.refresh_rate);
//     cJSON_AddNumberToObject(root, "set_point", set.set_point);
//     cJSON_AddNumberToObject(root, "mode", set.mode);
//     cJSON_AddBoolToObject(root, "refresh_rate", set.timestamp);
//     cJSON_AddNumberToObject(root, "mode", set.interval);
//     cJSON_AddBoolToObject(root, "refresh_rate", set.file_size);

//     if (cJSON_PrintPreallocated(root, buff, len, true) == 0)
//     {
//         //ESP_LOGE(TAG, "getSettingsStr(): failed to parse json into string");
//         cJSON_Delete(root);
//         return ESP_FAIL;
//     }
//     cJSON_Delete(root);
//     return ESP_OK;
// }

// //
// esp_err_t System::getSettingsStruct(settings *set, const char *json_str)
// {
//     cJSON *root = NULL, *graph_points = NULL, *refresh_rate = NULL, *set_point = NULL, *mode = NULL, *timestamp = NULL, *interval = NULL, *file_size = NULL, *datetime = NULL;
//     char buff[SETTINGS_BUFFER];

//     memset(buff, 0, SETTINGS_BUFFER);
//     strncpy(buff, json_str, SETTINGS_BUFFER);

//     root = cJSON_Parse(buff);
//     if (root == NULL)
//     {
//         ESP_LOGE(TAG, "getSettingsStruct(): parsing error");
//         return ESP_FAIL;
//     }

//     graph_points = cJSON_GetObjectItem(root, "graph_points");
//     refresh_rate = cJSON_GetObjectItem(root, "refresh_rate");
//     set_point = cJSON_GetObjectItem(root, "set_point");
//     mode = cJSON_GetObjectItem(root, "mode");
//     timestamp = cJSON_GetObjectItem(root, "timestamp");
//     interval = cJSON_GetObjectItem(root, "interval");
//     file_size = cJSON_GetObjectItem(root, "file_size");

//     if (!cJSON_IsNumber(graph_points) || !cJSON_IsNumber(refresh_rate) || !cJSON_IsNumber(set_point) || !cJSON_IsNumber(mode) || !cJSON_IsBool(timestamp) || !cJSON_IsNumber(interval) || !cJSON_IsNumber(file_size))
//     {
//         cJSON_Delete(root);
//         ESP_LOGE(TAG, "getSettingsStruct(): error checking json fields failed");
//         return ESP_FAIL;
//     }
//     set->graph_points = graph_points->valuedouble;
//     set->refresh_rate = refresh_rate->valuedouble;
//     set->set_point = set_point->valuedouble;
//     set->mode = mode->valuedouble;
//     set->timestamp = (cJSON_IsTrue(timestamp) == 1) ? true : false;
//     set->interval = interval->valuedouble;
//     set->file_size = file_size->valuedouble;

//     // setting datetime
//     datetime = cJSON_GetObjectItem(root, "datetime");
//     tm new_time, temp;
//     if ( cJSON_IsString(datetime) )
//     {
//         memset(buff, 0, sizeof(buff));
//         strncpy(buff, datetime->valuestring, sizeof(buff));
//         if (this->getTimeStruct(&new_time, buff) == ESP_OK)
//         {

//             if ( this->setTimeRTC(new_time) == ESP_OK ) {
//                 this->setTime(new_time);
//             }
//             else
//                 ESP_LOGE(TAG, "getSettingsStruct(): failed to set RTC time");
//         }
//         else
//             ESP_LOGE(TAG, "getSettingsStruct(): failed to parse datetime");
//     }
//     cJSON_Delete(root);
//     return ESP_OK;
// }

// // populates internal structure @_set with data from flash
// esp_err_t System::updateSettings(void)
// {
//     char buff[SETTINGS_BUFFER];
//     settings set;
//     memset(buff, 0, sizeof(buff));

//     SEMAPHORE_TAKE();
//     FILE *fptr = fopen(SETTINGS_PATH, "r");
//     if (fptr == NULL)
//     {
//         ESP_LOGE(TAG, "updateSettings(): failed to open settings file");
//         SEMAPHORE_GIVE();
//         return ESP_FAIL;
//     }

//     fread(buff, sizeof(buff), 1, fptr);
//     if (this->getSettingsStruct(&set, buff) != ESP_OK)
//     {
//         ESP_LOGE(TAG, "updateSettings(): failed to parse back settings from file");
//         fclose(fptr);
//         SEMAPHORE_GIVE();
//         return ESP_FAIL;
//     }
//     fclose(fptr);
//     this->_set = set;
//     SEMAPHORE_GIVE();
//     return ESP_OK;
// }

// // write settings json string @json_str to flash file
// esp_err_t System::setSettings(const char *json_str)
// {
//     char buff[SETTINGS_BUFFER];
//     memset(buff, 0, SETTINGS_BUFFER);
//     strncpy(buff, json_str, SETTINGS_BUFFER);

//     SEMAPHORE_TAKE();
//     // write
//     FILE *fptr = fopen(SETTINGS_PATH, "w");
//     if (fptr == NULL)
//     {
//         ESP_LOGE(TAG, "setSettings(): failed to open settings file");
//         SEMAPHORE_GIVE();
//         return ESP_FAIL;
//     }
//     if (fprintf(fptr, "%s", buff) < 0)
//         ESP_LOGW(TAG, "setSettings(): error in fprintf function");
//     fclose(fptr);
//     SEMAPHORE_GIVE();

//     this->updateSettings();
//     return ESP_OK;
// }

// //
// esp_err_t System::dumpSettinsStr(char *buff, size_t len)
// {
//     memset(buff, 0, len);
//     SEMAPHORE_TAKE();
//     FILE *fptr = fopen(SETTINGS_PATH, "r");
//     if (fptr == NULL)
//     {
//         ESP_LOGE(TAG, "updateSettings(): failed to open settings file");
//         SEMAPHORE_GIVE();
//         return ESP_FAIL;
//     }
//     fread(buff, 1, len, fptr);
//     fclose(fptr);
//     SEMAPHORE_GIVE();
//     return ESP_OK;
// }