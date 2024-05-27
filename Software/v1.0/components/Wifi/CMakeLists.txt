idf_component_register(SRCS "Wifi.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash esp_event System
                    )