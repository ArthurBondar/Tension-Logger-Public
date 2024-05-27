/*




*/

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
//#include "mdns.h" // if used add cMakeTxt


#include "lwip/err.h"
#include "lwip/sys.h"

#include "System.h"


#define ESP_WIFI_AP        "TensionLogger"
//#define ESP_MAXIMUM_RETRY  20
#define ESP_WIFI_CHANNEL   6
#define MAX_STA_CONN       5

class Wifi
{
public:
    Wifi(/* args */);
    ~Wifi();
    esp_err_t init();
    esp_err_t deinit(void);

private:
    //void event_handler(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data);
    esp_err_t wifi_init();
    //esp_err_t wifi_init_softap(void);
    esp_err_t init_nvs(void);
    //void init_mdns(void);

};

