/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "Flash.h"
#include "esp_log.h"

static const char *TAG = "main";

extern "C" {
    void app_main();
}

void app_main(void)
{
    Flash mem;

    ESP_LOGI(TAG, "APP START");

    int ret = mem.init();
    if (ret != SUCCESS)
    {
        ESP_LOGI(TAG, "Card Mount Failed return code: %d\n", ret);
        vTaskDelete(NULL);
    }

    uint32_t ws = 0, rs = 0;
    ret = mem.testFileIO("speed.txt", &ws, &rs);
    if (ret == SUCCESS)
        ESP_LOGI(TAG, "write speed = %dB/s\nreed speed = %dB/s", 1024 * 1024 / ws, 1024 * 1024 / rs);
    else
        ESP_LOGI(TAG, "Test IO speed failed ret = %d", ret);

    //  createDir(SD_MMC, "/mydir");
    //  listDir(SD_MMC, "/", 0);
    //  removeDir(SD_MMC, "/mydir");
    //  listDir(SD_MMC, "/", 2);
    //  writeFile(SD_MMC, "/hello.txt", "Hello ");
    //  appendFile(SD_MMC, "/hello.txt", "World!\n");
    //  readFile(SD_MMC, "/hello.txt");
    //  deleteFile(SD_MMC, "/foo.txt");
    //  renameFile(SD_MMC, "/hello.txt", "/foo.txt");
    //  readFile(SD_MMC, "/foo.txt");
    //  testFileIO(SD_MMC, "/test.txt");

    ESP_LOGI(TAG, "APP FINISH");
    vTaskDelete(NULL);
}
