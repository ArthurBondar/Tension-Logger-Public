/*

  SDCard calls use the following sequence:
  1. Mount the SD card (checks CD pin) + takes semaphore
  2. Open file - saves file pointer internally
  3. Write / Read / List Dir
  4. Umount - releases the semaphore

*/

#include "SDCard.h"

#define CHECK_CARD()                                    \
  do                                                    \
  {                                                     \
    if (this->checkCard() != ESP_OK)                    \
    {                                                   \
      ESP_LOGE(TAG, "CHECK_CARD(): card not peresent"); \
      return ESP_FAIL;                                  \
    }                                                   \
  } while (0)

#define CHECK_MOUNTED()                                            \
  do                                                               \
  {                                                                \
    if (this->mounted == false)                                    \
    {                                                              \
      ESP_LOGW(TAG, "CHECK_MOUNTED(): card call without mount()"); \
      return ESP_FAIL;                                             \
    }                                                              \
  } while (0)

static const char *TAG = "SDCard";

/* Null, because instance will be initialized on demand. */
SDCard *SDCard::inst = 0;

//
SDCard::SDCard()
{
  ESP_LOGI(TAG, "SDCard constructor");
  this->xSemaphore = xSemaphoreCreateMutex();
  if (this->xSemaphore == NULL)
    ESP_LOGE(TAG, "SDCard(): failed to create semaphore");

  strncpy(this->_filename, "2021-02-13_18-35-00", sizeof(this->_filename));
}

//
SDCard *SDCard::instance(void)
{
  if (inst == 0)
  {
    ESP_LOGI(TAG, "SDCard(): creating instance");
    inst = new SDCard();
  }
  return inst;
}

// Check if the card is present using Card Detect pin of the SD socket
esp_err_t SDCard::checkCard(void)
{
  if (gpio_get_level(this->_cd_pin) == 0) // Pin HIGH - no CARD
  {
    System::instance()->clearErrorFlag(disk_not_found);
    return ESP_OK;
  }
  else
  {
    System::instance()->setErrorFlag(disk_not_found);
    return ESP_FAIL;
  }
}

// Mounting the SD card
esp_err_t SDCard::init(uint8_t cd_pin)
{
  this->_cd_pin = (gpio_num_t)cd_pin;

  // configure CD pin
  this->io_conf.intr_type = GPIO_INTR_DISABLE;
  this->io_conf.mode = GPIO_MODE_INPUT;
  this->io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
  this->io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
  uint32_t mask = (uint32_t)this->_cd_pin;
  this->io_conf.pin_bit_mask = 1ULL << mask; // bit mask: GPIO_SD_CD (27)

  if (gpio_config(&this->io_conf) != ESP_OK)
  {
    ESP_LOGE(TAG, "init(): CD pin configure failed");
    return ESP_FAIL;
  }

  // configure slot
  this->slot_config.width = 4;
  //slot_config.gpio_cd = (gpio_num_t) 27;

  // configure host
  this->host.flags = SDMMC_HOST_FLAG_4BIT;
  this->host.max_freq_khz = SDMMC_FREQ_HIGHSPEED;

  // configure FAT
  this->mount_config.format_if_mount_failed = false;
  this->mount_config.max_files = 2;
  this->mount_config.allocation_unit_size = 0;

  return ESP_OK;
}

// Mounting the SD card
esp_err_t SDCard::mount()
{
  CHECK_CARD();
  SEMAPHORE_TAKE(); // semaphore released by umount() function

  // mount
  esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_CARD_MOUNT_POINT, &this->host, &this->slot_config, &this->mount_config, &this->_card);
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "init(): Failed to initialize the card (%s)", esp_err_to_name(ret));
    SEMAPHORE_GIVE();
    return ESP_FAIL;
  }
  // DEBUG
  //sdmmc_card_print_info(stdout, this->_card);
  //ESP_LOGI(TAG, "init(): card mounted");
  this->mounted = true;
  return ESP_OK;
}

// populates the card space structure with current sd data in KB !!
esp_err_t SDCard::getCardSpace(SDCardSpace *card_space)
{
  // if (!this->_card || this->checkCard() != ESP_OK)
  //   return ESP_FAIL;

  FATFS *fs;
  DWORD fre_clust, free_sect, tot_sect;
  const char *type;

  if (f_getfree(SD_CARD_MOUNT_POINT, &fre_clust, &fs) != 0)
    return ESP_FAIL; // Get volume information and free clusters of drive
  tot_sect = (fs->n_fatent - 2) * fs->csize;
  free_sect = fre_clust * fs->csize;

  card_space->cardSize = ((uint64_t)this->_card->csd.capacity * this->_card->csd.sector_size) / 1024;
  card_space->totalBytes = (uint64_t)tot_sect / 2;
  card_space->freeBytes = (uint64_t)free_sect / 2;

  if (this->_card->is_sdio)
    type = "SDIO";
  else if (this->_card->is_mmc)
    type = "MMC";
  else
    type = (this->_card->ocr & SD_OCR_SDHC_CAP) ? "SDHC/SDXC" : "SDSC";

  snprintf(card_space->name, CARD_NAME, "%s (%s)", this->_card->cid.name, type);
  //ESP_LOGI(TAG, "Name:%s Size: %llu B Total: %llu B Free: %llu B", card_space->name, card_space->cardSize, card_space->totalBytes, card_space->freeBytes);

  return ESP_OK;
}

// unmount the SD card
esp_err_t SDCard::unmount(void)
{
  //CHECK_CARD();
  esp_err_t ret = esp_vfs_fat_sdcard_unmount(SD_CARD_MOUNT_POINT, this->_card);
  SEMAPHORE_GIVE();
  if (ret != ESP_OK)
  {
    ESP_LOGE(TAG, "umount(): VFS-FAT-SDMMC unmount failed (%s)", esp_err_to_name(ret));
    System::instance()->setErrorFlag(internal_error);
  }
  this->mounted = false;
  return ret;
}

// get file info
esp_err_t SDCard::_getStat(const char *path, struct stat *_stat)
{
  char *temp = (char *)malloc(strlen(path) + strlen(SD_CARD_MOUNT_POINT) + 2);
  if (!temp)
  {
    ESP_LOGE(TAG, "_getStat(): malloc failed");
    return ESP_FAIL;
  }

  sprintf(temp, "%s/%s", SD_CARD_MOUNT_POINT, path);
  int ret = stat(temp, _stat);
  if (ret == -1)
  {
    free(temp);
    ESP_LOGW(TAG, "_getStat(): stat returned %d", ret);
    return ESP_ERR_INVALID_STATE;
  }

  free(temp);
  return ESP_OK;
}

// Populates a structure list of files with data. n = number of files found
esp_err_t SDCard::listDir(SDCardFile **list, int *file_num)
{
  DIR *dp;
  struct dirent *ep;
  struct stat _stat;

  CHECK_MOUNTED();

  dp = opendir(SD_CARD_MOUNT_POINT);
  if (dp == NULL)
    return ESP_FAIL;

  this->clearFileList(); // clear previously allocated memory !!

  while ((ep = readdir(dp)) != NULL)
  {
    if (this->_getStat(ep->d_name, &_stat) == ESP_ERR_INVALID_STATE)
      continue;
    //ESP_LOGI(TAG, "file:%s st_dev: %hi st_ino: %hi, st_mode:%i, st_nlink:%hi, st_uid:%hi, st_gid:%hi, st_rdev:%hi, off_t:%li",
    //         ep->d_name, _stat.st_dev, _stat.st_ino, _stat.st_mode, _stat.st_nlink, _stat.st_uid, _stat.st_gid, _stat.st_rdev, _stat.st_size);
    if (S_ISREG(_stat.st_mode)) // if file
    {
      // Allocating memory for new file info structure
      SDCardFile *file_data = (SDCardFile *)malloc(sizeof(SDCardFile));
      if (!file_data)
      {
        ESP_LOGE(TAG, "listDir(): malloc failed\n");
        return ESP_ERR_NO_MEM;
      }
      memset(file_data->name, 0, CARD_NAME);
      strncpy(file_data->name, ep->d_name, sizeof(file_data->name) - 1);
      file_data->size = _stat.st_size;
      file_data->lastWrite = *(localtime(&_stat.st_mtime));
      // passing the pointer to the file into the structure
      this->_file_list[this->_file_num] = file_data;
      list[this->_file_num] = this->_file_list[this->_file_num];
      this->_file_num++;
    }
  }
  *file_num = this->_file_num;
  return ESP_OK;
}

// Private function used to de-allocate the file list memory
void SDCard::clearFileList(void)
{
  for (int i = 0; i < this->_file_num; i++)
    free(this->_file_list[i]);
  this->_file_num = 0;
}

// Open file for internally for read operation
esp_err_t SDCard::openFile(const char *path, const char *permission)
{
  CHECK_MOUNTED();
  char *temp = (char *)malloc(strlen(path) + strlen(SD_CARD_MOUNT_POINT) + 2);
  if (!temp)
    return ESP_ERR_NO_MEM;

  sprintf(temp, "%s/%s", SD_CARD_MOUNT_POINT, path);

  this->_file = fopen(temp, permission);
  if (!this->_file)
  {
    ESP_LOGE(TAG, "openFile(): failed to open file - %s\n", temp);
    free(temp);
    return ESP_FAIL;
  }
  // increasing file buffer size
  if (setvbuf(this->_file, NULL, _IOFBF, FILE_BUFFER) != 0)
  {
    ESP_LOGE(TAG, "openFile(): setvbuf failed\n"); // POSIX version sets errno
    free(temp);
    fclose(this->_file);
    return ESP_FAIL;
  }
  free(temp);
  return ESP_OK;
}

// Close file opened previously
esp_err_t SDCard::closeFile(void)
{
  CHECK_MOUNTED();
  int rc = fclose(this->_file);
  if (rc != 0)
  {
    ESP_LOGE(TAG, "closeFile(): fclose returned %d", rc);
    return ESP_FAIL;
  }
  return ESP_OK;
}

// Read file: path
ssize_t SDCard::readFile(char *buff, size_t len)
{
  if (!this->_file)
    return 0;
  return fread((uint8_t *)buff, 1, len, this->_file);
}

// write file - path
esp_err_t SDCard::writeFile(const char *message)
{
  CHECK_MOUNTED();
  if (!this->_file)
    return ESP_ERR_INVALID_STATE;
  if (fwrite(message, 1, strlen(message), this->_file))
    return ESP_OK;
  else
    return ESP_FAIL;
}

// Remove file from SD card
esp_err_t SDCard::deleteFile(const char *path)
{
  CHECK_MOUNTED();
  char *temp = (char *)malloc(strlen(path) + strlen(SD_CARD_MOUNT_POINT) + 2);
  esp_err_t err;
  if (!temp)
    return ESP_ERR_NO_MEM;

  sprintf(temp, "%s/%s", SD_CARD_MOUNT_POINT, path);

  if (remove(temp) != 0)
  {
    err = ESP_FAIL;
    ESP_LOGE(TAG, "deleteFile(): failed to delete %s", temp);
  }
  else
    err = ESP_OK;

  free(temp);
  return err;
}

// Test read and write speed to SD card - return in ms per 1MB
esp_err_t SDCard::testFileIO(const char *path, uint32_t *write_speed, uint32_t *read_speed)
{
  struct stat _stat;
  size_t len = 0, i = 0;

  char *buff = (char *)heap_caps_malloc(FILE_BUFFER, MALLOC_CAP_DMA);
  if (!buff)
  {
    ESP_LOGE(TAG, "testFileIO(): malloc failed\n");
    return ESP_FAIL;
  }

  // open file
  this->openFile(path, "w");
  if (!this->_file)
  {
    free(buff);
    return ESP_FAIL;
  }
  // populate buffer
  for (i = 0; i < FILE_BUFFER; i++)
    buff[i] = 'a';
  buff[FILE_BUFFER - 1] = 0;
  // Write speed test
  int64_t start = esp_timer_get_time();
  for (i = 0; i < 64; i++) // writing 1MB test file
    fwrite(buff, 1, FILE_BUFFER, this->_file);
  *write_speed = (esp_timer_get_time() - start) / 1000;
  fclose(this->_file);

  // read speed test
  this->openFile(path, "r");
  this->_getStat(path, &_stat);
  len = _stat.st_size;
  start = esp_timer_get_time();
  while (len)
  {
    size_t toRead = len;
    if (toRead > FILE_BUFFER)
      toRead = FILE_BUFFER;
    fread((uint8_t *)buff, 1, toRead, this->_file);
    len -= toRead;
  }
  *read_speed = (esp_timer_get_time() - start) / 1000;
  fclose(this->_file);
  free(buff);
  return ESP_OK;
}

// check if file exists
esp_err_t SDCard::checkFile(const char *filename)
{
  CHECK_MOUNTED();
  struct stat st;
  char *temp = (char *)malloc(strlen(filename) + strlen(SD_CARD_MOUNT_POINT) + 2);
  if (!temp)
    return ESP_ERR_NO_MEM;
  sprintf(temp, "%s/%s", SD_CARD_MOUNT_POINT, filename);

  int rc = stat(temp, &st);
  free(temp);

  if (rc == 0)
    return ESP_OK;

  return ESP_ERR_NOT_FOUND;
}

// // return filename of current storage file
// esp_err_t SDCard::getFileName(char *buff, size_t len)
// {
//   if (buff == NULL || len < MAX_FILE_NAME)
//     return ESP_ERR_INVALID_ARG;
//   memset(buff, 0, len);
//   //SEMAPHORE_TAKE();
//   strncpy(buff, this->_filename, len);
//   //SEMAPHORE_GIVE();
//   return ESP_OK;
// }

// // set filename of current storage file
// esp_err_t SDCard::setFileName(const char *new_name)
// {
//   //SEMAPHORE_TAKE();
//   memset(this->_filename, 0, sizeof(this->_filename));
//   strncpy(this->_filename, new_name, sizeof(this->_filename));
//   //SEMAPHORE_GIVE();
//   return ESP_OK;
// }

// get file size
// esp_err_t SDCard::getFileSize(const char *filename, uint64_t *size)
// {
//   CHECK_MOUNTED();
//   struct stat _stat;

//   if (this->_getStat(filename, &_stat) != ESP_OK)
//     return ESP_FAIL;
//   else
//     *size = _stat.st_size;

//   return ESP_OK;
// }