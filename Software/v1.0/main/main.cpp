/* Simple HTTP Server Example
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "Wifi.h"
#include "Server.h"
#include "System.h"
#include "Sensor.h"
#include "SDCard.h"
#include "Settings.h"

#include "freertos/freeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <sys/time.h>
#include <stdio.h>

//test
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>

#define MAIN_TASK_LOOP 1000
#define SENSOR_TASK_LOOP 10
#define SENSOR_TASK_SER_TIMEOUT 800
#define STORAGE_TASK_LOOP 1000
#define DEBUG_TASK_LOOP 15000
#define DATA_POINTS 30

#define FILE_HEADER "Datetime,Tension,Units\r\n"

extern "C"
{
    void app_main();
}
void sensor_task(void *pvParameters);
void storage_task(void *pvParameters);
void debug_task(void *pvParameters);
void saveData(SensorData *data, int len);
void receive_thread(void *pvParameters);

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "app_main(): started");
    BaseType_t xReturned;
    Wifi myWifi;
    Server myServer;
    System *system = System::instance();

    if (system->init() != ESP_OK)
        system->setErrorFlag(internal_error);

    if (Settings::instance()->init() != ESP_OK)
        system->setErrorFlag(internal_error);

    if (myWifi.init() != ESP_OK)
        system->setErrorFlag(internal_error);

    if (myServer.init() != ESP_OK)
        system->setErrorFlag(internal_error);

    if (Sensor::instance()->init() != ESP_OK)
    {
        system->setErrorFlag(internal_error);
        system->setErrorFlag(sensor_not_found);
    }

    if (SDCard::instance()->init() != ESP_OK)
    {
        system->setErrorFlag(internal_error);
        system->setErrorFlag(disk_not_found);
    }

    if (system->checkError() != ESP_OK)
        ESP_LOGE(TAG, "app_main(): error during initialisation");

    xReturned = xTaskCreatePinnedToCore(
        sensor_task,              /* Function that implements the task. */
        "sensor_task",            /* Text name for the task. */
        4096,                     /* Stack size in words, not bytes. */
        (void *)1,                /* Parameter passed into the task. */
        configMAX_PRIORITIES - 3, /* Priority at which the task is created. */
        (xTaskHandle *)NULL,      /* Used to pass out the created task's handle. */
        (BaseType_t)1);           /* Core ID ( PRO 0, APP 1 ) */
    if (xReturned != pdPASS)
        ESP_LOGE(TAG, "main(): failed to create sensor_task");

    xReturned = xTaskCreatePinnedToCore(
        storage_task,
        "storage_task",
        16384,
        (void *)1,
        configMAX_PRIORITIES - 2,
        (xTaskHandle *)NULL,
        (BaseType_t)1);
    if (xReturned != pdPASS)
        ESP_LOGE(TAG, "main(): failed to create storage_task");

    // xReturned = xTaskCreatePinnedToCore(
    //     debug_task,
    //     "debug_task",
    //     4096,
    //     (void *)1,
    //     configMAX_PRIORITIES - 1,
    //     (xTaskHandle *)NULL,
    //     (BaseType_t)1);
    // if (xReturned != pdPASS)
    //     ESP_LOGE(TAG, "main(): failed to create debug task");

    bool state = false;
    int delay = MAIN_TASK_LOOP;
    size_t free_mem = 0, total = 0, used = 0;
    float mem_perc = 0;

    system->blink(extr_led, 15, 150);
    while (1)
    {
        state = (!state) ? true : false;
        system->setIO(extr_led, state);
        // checking memory overflow
        free_mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
        used = total - free_mem;
        mem_perc = (float)used * 100 / total;
        if (mem_perc > MEM_OVERFLOW_PERC)
            system->setErrorFlag(memory_overflow);
        // checking for any errors
        if (system->checkError() == ESP_OK)
        {
            delay = MAIN_TASK_LOOP;
            system->setIO(intr_led_red, false);
            system->setIO(intr_led_green, true);
        }
        else
        {
            delay = MAIN_TASK_LOOP / 4;
            system->setIO(intr_led_red, true);
            system->setIO(intr_led_green, false);
        }
        vTaskDelay(pdMS_TO_TICKS(delay));
    }
    vTaskDelete(NULL);
}

//
void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "sensor_task(): started");

    Sensor *sensor = Sensor::instance();
    System *system = System::instance();
    esp_err_t rc;

    sensor->flush();
    while (1)
    {
        rc = sensor->readSerial(SENSOR_TASK_SER_TIMEOUT);
        if (rc == ESP_ERR_NOT_FOUND)
            system->setErrorFlag(sensor_not_found);
        else
            system->clearErrorFlag(sensor_not_found);
        if (rc == ESP_ERR_INVALID_RESPONSE)
            system->setErrorFlag(parsing_error);
        else
            system->clearErrorFlag(parsing_error);
    }

    vTaskDelete(NULL);
}

//
void storage_task(void *pvParameters)
{
    ESP_LOGI(TAG, "storage_task(): started");

    Sensor *sensor = Sensor::instance();
    Settings *_settings = Settings::instance();
    System *system = System::instance();
    SDCard *card = SDCard::instance();
    SensorData data_points[DATA_POINTS] = {SENSOR_DEFAULTS()};
    int sec_count = 0, index = 0, interval_s = 1;
    double f_interval = 1;
    char file_name[MAX_FILE_NAME];
    tm _time = TIME_DEFAULTS();
    esp_err_t rc;

    snprintf(file_name, sizeof(file_name), "%s", "temporary.csv");
    vTaskDelay(pdMS_TO_TICKS(10 * 1000));
    while (1)
    {
        if (!system->getErrorFlag(sensor_not_found) && !system->getErrorFlag(parsing_error))
        {
            _settings->getParameter(&f_interval, "interval");
            interval_s = (int)f_interval;
            sec_count++;
            // time to take a reading into internal buffer
            if (sec_count >= interval_s)
            {
                sec_count = 0;
                if (sensor->getData(&data_points[index]) != ESP_OK)
                    continue;
                index++;
                if (index >= (DATA_POINTS / interval_s)) // save operation
                {
                    if (card->mount() == ESP_OK)
                    {
                        // 1. check file exists
                        rc = card->checkFile(file_name);
                        if (rc == ESP_ERR_NOT_FOUND) // file doesnt exist
                        {
                            ESP_LOGI(TAG, "storage_task(): file doesnt exists, creating..");
                            system->getTime(&_time);
                            system->getTimeString(file_name, sizeof(file_name), FILENAME_FORMAT, _time);
                            if (card->openFile(file_name, "w") == ESP_OK)
                            { // 2. create file write headers
                                card->writeFile(FILE_HEADER);
                                saveData(data_points, index);
                                card->closeFile();
                            }
                            else
                                ESP_LOGE(TAG, "storage_task(): failed to create file");
                        }
                        else if (rc == ESP_OK) // file exists
                        {
                            //ESP_LOGI(TAG, "storage_task(): file exists, writing..");
                            if (card->openFile(file_name, "a") == ESP_OK)
                            {
                                saveData(data_points, index);
                                card->closeFile();
                            }
                        } // file operation end
                        if (card->unmount() != ESP_OK)
                            ESP_LOGE(TAG, "storage_task(): card unmount failed");
                    } // card mount end
                    index = 0;
                } // second counter buff - end
                if (index >= DATA_POINTS)
                    index = 0;
            } // first counter, sec - end
        }
        card->checkCard();
        vTaskDelay(pdMS_TO_TICKS(STORAGE_TASK_LOOP));
    }
    vTaskDelete(NULL);
}

// function to save sensor data to USB
void saveData(SensorData *data_points, int len)
{
    System *system = System::instance();
    SDCard *card = SDCard::instance();
    char buff[LINE_BUFFER], time_buff[TIME_LEN];

    for (int i = 0; i < len; i++)
    { //write data lines
        system->getTimeString(time_buff, sizeof(time_buff), TIME_FORMAT_JS, data_points[i].timestamp);
        if (data_points[i].peak_tension == -1)
            snprintf(buff, sizeof(buff), "%s,%.1f,%s\n", time_buff, data_points[i].tension, data_points[i].units);
        else
            snprintf(buff, sizeof(buff), "%s,%.1f,%.1f,%s\n", time_buff, data_points[i].tension, data_points[i].peak_tension, data_points[i].units);

        //ESP_LOGI(TAG, "storage_task(): %s", buff);
        card->writeFile(buff);
    }
}

//
void debug_task(void *pvParameters)
{
    ESP_LOGI(TAG, "debug_task(): started");
    size_t free_mem = 0, total = 0, used = 0;
    tm now;
    char time_str[TIME_LEN], cpu_stat[512];

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(DEBUG_TASK_LOOP));
        //heap_caps_print_heap_info(MALLOC_CAP_8BIT); // MALLOC_CAP_8BIT
        free_mem = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        total = heap_caps_get_total_size(MALLOC_CAP_8BIT);
        used = total - free_mem;
        System::instance()->getTime(&now);
        memset(time_str, 0, TIME_LEN);
        System::instance()->getTimeString(time_str, TIME_LEN, TIME_FORMAT_SEC, now);
        ESP_LOGI(TAG, "debug_task(): %s %.2fKB / %.2fKB (%.2f%%)", time_str, (float)used / 1024.0, (float)total / 1024, (float)used * 100 / total);
        // CPU
        vTaskGetRunTimeStats(cpu_stat);
        ESP_LOGI(TAG, "debug_task(): CPU usage -----\n%s", cpu_stat);
    }
    vTaskDelete(NULL);
}

// void receive_thread(void *pvParameters)
// {
//     int socket_fd;
//     struct sockaddr_in sa, ra;

//     socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (socket_fd < 0)
//     {
//         ESP_LOGE(TAG, "Failed to create socket");
//         exit(0);
//     }

//     memset(&sa, 0, sizeof(struct sockaddr_in));

//     tcpip_adapter_ip_info_t ip;
//     tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_AP, &ip);
//     ra.sin_family = AF_INET;
//     ra.sin_addr.s_addr = ip.ip.addr;
//     ra.sin_port = htons(53);
//     if (bind(socket_fd, (struct sockaddr *)&ra, sizeof(struct sockaddr_in)) == -1)
//     {
//         ESP_LOGE(TAG, "Failed to bind to 53/udp");
//         close(socket_fd);
//         exit(1);
//     }

//     struct sockaddr_in client;
//     socklen_t client_len;
//     client_len = sizeof(client);
//     int length;
//     char data[80];
//     char response[100];
//     char ipAddress[INET_ADDRSTRLEN];
//     int idx;
//     int err;

//     ESP_LOGI(TAG, "DNS Server listening on 53/udp");
//     while (1)
//     {
//         length = recvfrom(socket_fd, data, sizeof(data), 0, (struct sockaddr *)&client, &client_len);
//         if (length > 0)
//         {
//             data[length] = '\0';

//             inet_ntop(AF_INET, &(client.sin_addr), ipAddress, INET_ADDRSTRLEN);
//             ESP_LOGI(TAG, "Replying to DNS request (len=%d) from %s", length, ipAddress);

//             // Prepare our response
//             response[0] = data[0];
//             response[1] = data[1];
//             response[2] = 0b10000100 | (0b00000001 & data[2]); //response, authorative answer, not truncated, copy the recursion bit
//             response[3] = 0b00000000;                          //no recursion available, no errors
//             response[4] = data[4];
//             response[5] = data[5]; //Question count
//             response[6] = data[4];
//             response[7] = data[5]; //answer count
//             response[8] = 0x00;
//             response[9] = 0x00; //NS record count
//             response[10] = 0x00;
//             response[11] = 0x00; //Resource record count

//             memcpy(response + 12, data + 12, length - 12); //Copy the rest of the query section
//             idx = length;

//             // Prune off the OPT
//             // FIXME: We should parse the packet better than this!
//             if ((response[idx - 11] == 0x00) && (response[idx - 10] == 0x00) && (response[idx - 9] == 0x29))
//                 idx -= 11;

//             //Set a pointer to the domain name in the question section
//             response[idx] = 0xC0;
//             response[idx + 1] = 0x0C;

//             //Set the type to "Host Address"
//             response[idx + 2] = 0x00;
//             response[idx + 3] = 0x01;

//             //Set the response class to IN
//             response[idx + 4] = 0x00;
//             response[idx + 5] = 0x01;

//             //A 32 bit integer specifying TTL in seconds, 0 means no caching
//             response[idx + 6] = 0x00;
//             response[idx + 7] = 0x00;
//             response[idx + 8] = 0x00;
//             response[idx + 9] = 0x00;

//             //RDATA length
//             response[idx + 10] = 0x00;
//             response[idx + 11] = 0x04; //4 byte IP address

//             //The IP address
//             response[idx + 12] = 192;
//             response[idx + 13] = 168;
//             response[idx + 14] = 1;
//             response[idx + 15] = 1;

//             err = sendto(socket_fd, response, idx + 16, 0, (struct sockaddr *)&client, client_len);
//             if (err < 0)
//             {
//                 ESP_LOGE(TAG, "sendto failed: %s", strerror(errno));
//             }
//         }
//     }
//     close(socket_fd);
// }