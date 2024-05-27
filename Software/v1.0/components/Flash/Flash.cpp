/*




*/


#include "Flash.h"
#include "esp_log.h"

static const char *TAG = "Flash";

// Mounting the SD card
uint8_t Flash::init( void )
{
  ESP_LOGI(TAG, "initializing SPIFFS");

  esp_vfs_spiffs_conf_t conf = {
    .base_path = MOUNT_POINT,
    .partition_label = NULL,
    .max_files = 2,
    .format_if_mount_failed = false
  };

  esp_err_t ret = esp_vfs_spiffs_register(&conf);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) ESP_LOGE(TAG, "init(): Failed to mount or format filesystem");
    else if (ret == ESP_ERR_NOT_FOUND) ESP_LOGE(TAG, "init(): Failed to find SPIFFS partition");
    else ESP_LOGE(TAG, "init(): Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
    return FAILED;
  }

  size_t total = 0, used = 0;
  ret = esp_spiffs_info(conf.partition_label, &total, &used);
  if (ret != ESP_OK) ESP_LOGE(TAG, "init(): Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
  else ESP_LOGI(TAG, "init(): Partition size: total: %d, used: %d", total, used);

  return SUCCESS;
}

uint8_t Flash::_getStat(const char *path, struct stat *_stat)
{
    char *temp = (char *) malloc( strlen(path)+strlen(this->_mount)+2 );
    if(!temp) {
      ESP_LOGE(TAG, "_getStat(): malloc failed");
      return FAILED;
    } 

    sprintf(temp, "%s/%s", this->_mount, path);
    int ret = stat(temp, _stat);
    if (ret == -1) {
      ESP_LOGW(TAG, "_getStat():stat returned %d", ret);
      free(temp);
      return NOT_A_FILE;
    }

    free(temp);
    return SUCCESS;
}

// Open file for internally for read operation
uint8_t Flash::openFile(const char *path, const char *permission)
{
  char *temp = (char *) malloc( strlen(path)+strlen(this->_mount)+2 );
  if(!temp) return FAILED;

  sprintf(temp, "%s/%s", this->_mount, path);

  this->_file = fopen(temp, permission);
  if (!this->_file) {
    ESP_LOGW(TAG, "openFile(): failed to open file - %s", temp);
    free(temp);
    return FAILED;
  }
  // increasing file buffer size
  if(setvbuf(this->_file, NULL, _IOFBF, FILE_BUFFER) != 0) {
    ESP_LOGE(TAG, "openFile(): setvbuf failed\n"); // POSIX version sets errno
    free(temp);
    return FAILED;
  }
  free(temp);
  return SUCCESS;
}

// Close file opened previously
uint8_t Flash::closeFile( void )
{
  if (!this->_file) return FAILED;
  fclose(this->_file);
  return SUCCESS;
}

// Read file: path
uint8_t Flash::readFile(char *buff, size_t len) 
{
  if (!this->_file) return FAILED;
  if( fread( (uint8_t *) buff, 1, len, this->_file) )
    return SUCCESS;
  else 
    return END_OF_FILE;
}

// write file - path
uint8_t Flash::writeFile(const char *message) 
{
  if (!this->_file) return FAILED;
  if ( fwrite(message, 1, strlen(message), this->_file) ) return SUCCESS;
  else return FAILED;
}

// Remove file from SD card
uint8_t Flash::deleteFile(const char *path) 
{
  if (remove(path)) return SUCCESS;
  else return FAILED;
}

// Test read and write speed to SD card - return in ms per 1MB
uint8_t Flash::testFileIO(const char *path, uint32_t *write_speed, uint32_t *read_speed) 
{
  struct stat _stat;
  size_t len = 0, i = 0;

  char* buff = (char *) heap_caps_malloc( FILE_BUFFER, MALLOC_CAP_DMA);
  if(!buff) {
    ESP_LOGE(TAG, "testFileIO(): malloc failed");
    return FAILED;
  }

  // open file
  this->openFile(path, "w");
  if (!this->_file) {
    free(buff);
    return FAILED;
  }
  // populate buffer
  for (i = 0; i < FILE_BUFFER; i++) buff[i] = 'a';
  buff[FILE_BUFFER-1] = 0;
  // Write speed test
  uint32_t start = esp_timer_get_time();
  for (i = 0; i < 64; i++)  // writing 1MB test file
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
    if (toRead > FILE_BUFFER) toRead = FILE_BUFFER;
    fread( (uint8_t *) buff, 1, toRead, this->_file);
    len -= toRead;
  }
  *read_speed = (esp_timer_get_time() - start) / 1000;
  fclose(this->_file);
  free(buff);
  return SUCCESS;
}
