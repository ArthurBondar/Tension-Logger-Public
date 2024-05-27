/**************************************************************************/
/*!
  @file     Flash.h

  Wrapper Class for communcating with SD card throught SD MMC interface

*/
/**************************************************************************/

#ifndef _FLASH_H_
#define _FLASH_H_

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h> // strlen
#include <stdlib.h>

#include "esp_timer.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"

#define SUCCESS 0
#define FAILED  1
#define END_OF_FILE 5
#define NOT_A_FILE 6

#define MAX_FILE_NAME 10
#define MAX_FILE_LIST 100

#define FILE_BUFFER   512     // buffer for read and write - 16 * 1024 - 16KB

#define MOUNT_POINT "/spiffs"


class Flash
{
public:
  uint8_t init(void);
  uint8_t openFile(const char *path, const char *permission);
  uint8_t closeFile( void );
  uint8_t readFile(char *buff, size_t len);
  uint8_t writeFile(const char *message);
  uint8_t deleteFile(const char *path);
  uint8_t testFileIO(const char *path, uint32_t *write_speed, uint32_t *read_speed);

private:
  int _file_num = -1;
  FILE *_file;
  const char *_mount = MOUNT_POINT;
  uint8_t _getStat(const char *path, struct stat *_stat);

};

#endif // _RTCLIB_H_
