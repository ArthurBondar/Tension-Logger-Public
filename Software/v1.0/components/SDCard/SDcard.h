/**************************************************************************/
/*!
  @file     SDCard.h

  Wrapper Class for communcating with SD card throught SD MMC interface

*/
/**************************************************************************/

#ifndef SDCARD_H
#define SDCARD_H

#include <time.h>

#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_vfs_fat.h"
#include "esp_vfs.h"

#include "driver/sdmmc_types.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "driver/gpio.h"
#include "sdmmc_cmd.h"

#include "System.h"

#define SD_CARD_MOUNT_POINT "/sdcard"

#define MAX_FILE_NAME 25
#define MAX_FILE_LIST 100

#define FILE_BUFFER 4096 // buffer for read and write - 16 * 1024 - 16KB
#define LINE_BUFFER 128

#define CARD_NAME 20
#define CD_PIN 27 //* Pin for card detection
//#define NOT_A_FILE 10

struct SDCardSpace
{
  char name[CARD_NAME];
  uint64_t cardSize;
  uint64_t totalBytes;
  uint64_t freeBytes;
};

struct SDCardFile
{
  char name[MAX_FILE_NAME];
  uint64_t size;
  struct tm lastWrite;
};

typedef enum
{
  CARD_NONE,
  CARD_MMC,
  CARD_SD,
  CARD_SDHC,
  CARD_UNKNOWN
} sdcard_type_t;

class SDCard
{
public:
  static SDCard *instance(void);
  esp_err_t init(uint8_t cd_pin = CD_PIN);
  esp_err_t mount(void);
  esp_err_t checkCard(void);
  esp_err_t getCardSpace(SDCardSpace *card_space);
  esp_err_t unmount(void);
  esp_err_t listDir(SDCardFile **list, int *file_num);
  esp_err_t openFile(const char *path, const char *permission);
  esp_err_t closeFile(void);
  ssize_t readFile(char *buff, size_t len);
  esp_err_t writeFile(const char *message);
  esp_err_t deleteFile(const char *path);
  esp_err_t testFileIO(const char *path, uint32_t *write_speed, uint32_t *read_speed);
  void clearFileList(void);
  //esp_err_t getFileName(char *buff, size_t len);
  //esp_err_t setFileName(const char *new_name);
  esp_err_t checkFile(const char *filename);
  //esp_err_t getFileSize(const char *filename, uint64_t *size);

private:
  static SDCard *inst;
  SDCard();
  SDCard(const SDCard *obj);
  SemaphoreHandle_t xSemaphore = NULL;

  gpio_config_t io_conf;
  gpio_num_t _cd_pin;

  sdmmc_card_t *_card;
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  esp_vfs_fat_sdmmc_mount_config_t mount_config;

  bool mounted = false;
  int _file_num = 0;
  FILE *_file;
  SDCardFile *_file_list[MAX_FILE_LIST];
  esp_err_t _getStat(const char *path, struct stat *_stat);
  char _filename[MAX_FILE_NAME];
};


#endif // _RTCLIB_H_
