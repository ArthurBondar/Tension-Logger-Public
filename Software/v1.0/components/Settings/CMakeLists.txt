idf_component_register(
    SRCS "Settings.cpp"
    INCLUDE_DIRS "."
    REQUIRES json fatfs vfs log freertos System
)