/*




*/
#ifndef SERVER_H
#define SERVER_H

#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fnmatch.h>
#include <fcntl.h>
#include <sys/time.h>
#include <inttypes.h>

#include "esp_http_server.h"
//#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_vfs.h"
// #include "esp_netif.h"
//#include "lwip/apps/netbiosns.h"
// #include "esp_event.h"
#include "esp_log.h"
#include "cJSON.h"

#include "System.h"
#include "Sensor.h"
#include "SDCard.h"
#include "Settings.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 64)
#define SCRATCH_BUFSIZE (16384) // 10240

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

class Server
{
public:
    Server(/* args */);
    ~Server();
    esp_err_t init();
    esp_err_t deinit(void);

private:
    //esp_err_t init_spiffs();
    esp_err_t start_server(const char *base_path);
    esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath);
    rest_server_context_t *rest_context;

};


#endif
