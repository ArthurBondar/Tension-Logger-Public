/*




*/

#include "Sensor.h"

static const char *TAG = "Sensor";

/* Null, because instance will be initialized on demand. */
Sensor *Sensor::inst = 0;
//
Sensor::Sensor()
{
    this->xSemaphore = xSemaphoreCreateMutex();
    if (this->xSemaphore == NULL)
        ESP_LOGE(TAG, "Sensor(): failed to create semaphore");
}

//
Sensor *Sensor::instance(void)
{
    if (inst == 0)
    {
        ESP_LOGI(TAG, "Sensor(): creating Sensor instance");
        inst = new Sensor();
    }
    return inst;
}

//
esp_err_t Sensor::init(void)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = DEFAULT_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, SERIAL_BUFF, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, 17, 16, 18, 23));
    ESP_ERROR_CHECK(uart_set_mode(UART_NUM_2, UART_MODE_UART));
    ESP_ERROR_CHECK(uart_flush(UART_NUM_2));
    ESP_ERROR_CHECK(uart_flush_input(UART_NUM_2));

    this->_initialized = true;
    ESP_LOGI(TAG, "init(): initialized");
    return ESP_OK;
}

//
void Sensor::dumpData(SensorData *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        ESP_LOGI(TAG, "[%d] time: %d-%02d-%02d %02d:%02d:%02d\t tension:%.1f \tpeak: %.1f \tunits: %s",
                 i,
                 data[i].timestamp.tm_year + 1900,
                 data[i].timestamp.tm_mon + 1,
                 data[i].timestamp.tm_mday,
                 data[i].timestamp.tm_hour,
                 data[i].timestamp.tm_min,
                 data[i].timestamp.tm_sec,
                 data[i].tension,
                 data[i].peak_tension,
                 data[i].units);
    }
}

//
esp_err_t Sensor::getData(SensorData *data_buff)
{
    esp_err_t rc = ESP_OK;
    SEMAPHORE_TAKE();
    if (this->_data.units == 0)
        rc = ESP_FAIL;
    else
        *data_buff = this->_data;
    SEMAPHORE_GIVE();
    return rc;
}

// read serial and store in internal buffer - return - ESP_ERR_NOT_FOUND, ESP_ERR_INVALID_RESPONSE
esp_err_t Sensor::readSerial(uint32_t delay)
{
    char buff[SERIAL_BUFF] = {0}, units[UNITS_LEN] = {0}, units2[UNITS_LEN] = {0};
    float tension = 0.0, peak = 0.0;
    int len = 0, end_byte = 0;
    time_t rawTime;

    //ESP_ERROR_CHECK( uart_get_buffered_data_len(UART_NUM_2, (size_t*)&len) );
    //memset(buff, 0, sizeof(buff));
    len = uart_read_bytes(UART_NUM_2, (uint8_t *)buff, SERIAL_BUFF, pdMS_TO_TICKS(delay));
    this->flush();
    buff[len] = '\0';
    //ESP_LOGI(TAG, "readSerial(): read [%d] %s", len, buff);
    if (len == 0 || buff[0] == 0) // sensor not found
        return ESP_ERR_NOT_FOUND;

    // parsing
    len = sscanf(buff, "%*d %*s %*d %*d:%*d:%*d %f %s %f %s %d", &tension, units, &peak, units2, &end_byte);
    if (len == 0) // datetime is not enabled
        len = sscanf(buff, "%f %s %f %s %d", &tension, units, &peak, units2, &end_byte);
    //ESP_LOGI(TAG, "readSerial(): parsed -> param read: %d, tension: %.1f, units: %s, peak: %.1f, units: %s, end_byte: %d", len, tension, units, peak, units2, end_byte);

    // Error checking
    if ((strcmp(units, "lbf") && strcmp(units, "N") && strcmp(units, "kgf")) || len < 3)
    {
        ESP_LOGE(TAG, "readSerial(): parsing error [ %s ]", buff);
        return ESP_ERR_INVALID_RESPONSE;
    }

    SEMAPHORE_TAKE();
    if (len == 3) // sensor mode 1 or 2 - [tension/peak],[units]
    {
        this->_data.tension = tension;
        this->_data.peak_tension = -1;
    }
    else if (len == 5) // sensor mode 3 [tension],[units],[peak],[unit2]
    {
        this->_data.tension = tension;
        this->_data.peak_tension = peak;
    }
    snprintf(this->_data.units, UNITS_LEN, "%s", units);
    time(&rawTime);
    this->_data.timestamp = *(localtime(&rawTime));
    SEMAPHORE_GIVE();

    return ESP_OK;
}

//
esp_err_t Sensor::setBaud(uint32_t baud)
{
    if (uart_set_baudrate(UART_NUM_2, baud) != ESP_OK)
    {
        ESP_LOGE(TAG, "setBaud(): baudrate set ESP_FAIL");
        return ESP_FAIL;
    }
    return ESP_OK;
}

//
void Sensor::deinit()
{
    ESP_ERROR_CHECK(uart_driver_delete(UART_NUM_2));
    this->_initialized = false;
}

//
void Sensor::flush(void)
{
    esp_err_t rc = uart_flush_input(UART_NUM_2);
    if (rc != ESP_OK)
        ESP_LOGE(TAG, "flush(): failed to flush ret = %s", esp_err_to_name(rc));
}

// //
// uint8_t Sensor::getMode(void)
// {
//     return this->_mode;
// }

// //
// esp_err_t Sensor::setMode(uint8_t mode)
// {
//     if (mode > 0 && mode < 6)
//     {
//         SEMAPHORE_TAKE();
//         this->_mode = mode;
//         SEMAPHORE_GIVE();
//         return ESP_OK;
//     }
//     else
//         return ESP_FAIL;
// }