idf_component_register(
    SRCS "System.cpp"
    INCLUDE_DIRS "."
    REQUIRES ds3231 i2cdev spiffs fatfs vfs json log esp_idf_lib_helpers esp_adc_cal freertos
)