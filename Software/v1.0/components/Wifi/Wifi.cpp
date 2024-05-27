/*




*/

#include "Wifi.h"

static const char *TAG = "Wifi";

//
Wifi::Wifi(/* args */)
{
}

//
Wifi::~Wifi()
{
}

//
static void event_handler(void *args, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
        System::instance()->blink(extr_led, 15, 150);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
        System::instance()->blink(extr_led, 15, 150);
    }
}

//
esp_err_t Wifi::wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *netif = esp_netif_create_default_wifi_ap();
    //esp_netif_dhcp_status_t netif_stat;
    esp_netif_ip_info_t ip_info;
    // set IP address
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(netif));
    esp_netif_set_ip4_addr(&ip_info.ip, 1, 1, 1, 1);
    esp_netif_set_ip4_addr(&ip_info.gw, 1, 1, 1, 1);
    esp_netif_set_ip4_addr(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(netif));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,
                                               IP_EVENT_STA_GOT_IP,
                                               &event_handler,
                                               NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));


    wifi_config_t wifi_config = {};
    strcpy((char *)wifi_config.ap.ssid, ESP_WIFI_AP);
    //wifi_config.ap.password = 0;
    wifi_config.ap.ssid_len = strlen(ESP_WIFI_AP);
    wifi_config.ap.channel = ESP_WIFI_CHANNEL;
    wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    wifi_config.ap.max_connection = MAX_STA_CONN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init(): AP created");
    return ESP_OK;
}

//
esp_err_t Wifi::init_nvs(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS init failed, retrying");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    return ret;
}

//
esp_err_t Wifi::init()
{
    if (this->init_nvs() != ESP_OK)
        return ESP_FAIL;
    if (this->wifi_init() != ESP_OK)
        return ESP_FAIL;
    return ESP_OK;
}

// //
// void Wifi::init_mdns(void)
// {
//     //ESP_LOGI(TAG, "init_mdns():");
//     ESP_ERROR_CHECK(mdns_init());
//     ESP_ERROR_CHECK(mdns_hostname_set(ESP_HOST_NAME));
//     ESP_ERROR_CHECK(mdns_instance_name_set("esp32-web-server"));

//     mdns_txt_item_t serviceTxtData[] = {
//         {"board", "esp32"},
//         {"path", "/"}};
//     ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData, 2));
//     //netbiosns_init();
//     //netbiosns_set_name(ESP_HOST_NAME);
//     ESP_LOGI(TAG, "init_mdns(): host name set to %s", ESP_HOST_NAME);
// }
