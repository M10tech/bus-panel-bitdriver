idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash
                    PRIV_REQUIRES UDPlogger esp_wifi esp_driver_i2s app_update mqtt json esp_timer esp_driver_gpio)
