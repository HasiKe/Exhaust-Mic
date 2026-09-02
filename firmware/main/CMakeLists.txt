idf_component_register(
    SRCS
        "main.c"
        "gopro.c"
        "zeitbasis.c"
        "pcm1863.c"
        "audio.c"
        "speicher.c"
        "bedienung.c"
        "i2cbus.c"
    INCLUDE_DIRS "."
    REQUIRES
        driver esp_driver_i2s esp_driver_i2c esp_driver_gpio esp_driver_ledc
        sdmmc esp_driver_sdmmc fatfs vfs
        bt nvs_flash esp_timer
)
