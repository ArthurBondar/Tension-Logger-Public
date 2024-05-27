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

#include "SDcard.h"

static const char *TAG = "main";

extern "C"
{
    void app_main();
}

void app_main(void)
{
    SDCard *sd = SDCard::instance();

    ESP_LOGI(TAG, "APP START");

    int ret = sd->init();
    if (ret != SUCCESS)
    {
        ESP_LOGI(TAG, "Card Mount Failed return code: %d\n", ret);
        vTaskDelete(NULL);
    }

    SDCardSpace cardMem;
    sd->getCardSpace(&cardMem);
    ESP_LOGI(TAG, "SD Card Size: %lluMB\nTotal: %lluB\nfree: %lluB\n", cardMem.cardSize, cardMem.totalBytes, cardMem.freeBytes);

    SDCardFile *files[MAX_FILE_NAME];
    int file_num = -1;
    ret = sd->listDir(&files[0], &file_num);
    if (ret != SUCCESS)
    {
        ESP_LOGI(TAG, "List Dir Failed return code: %d\n", ret);
        vTaskDelete(NULL);
    }

    for (; file_num >= 0; file_num--)
    {
        ESP_LOGI(TAG, "File [%d] name: %s,\t size:%llu,\t time: %d-%02d-%02d %02d:%02d:%02d\t",
                 file_num,
                 files[file_num]->name,
                 files[file_num]->size,
                 files[file_num]->lastWrite.tm_year + 1900,
                 files[file_num]->lastWrite.tm_mon + 1,
                 files[file_num]->lastWrite.tm_mday,
                 files[file_num]->lastWrite.tm_hour,
                 files[file_num]->lastWrite.tm_min,
                 files[file_num]->lastWrite.tm_sec);
    }

    uint32_t ws = 0, rs = 0;
    ret = sd->testFileIO("speed.txt", &ws, &rs);
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
