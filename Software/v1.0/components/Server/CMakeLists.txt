idf_component_register(
    SRCS "Server.cpp"
    INCLUDE_DIRS "."
    REQUIRES esp_http_server fatfs vfs json log System Sensor SDCard Settings
)