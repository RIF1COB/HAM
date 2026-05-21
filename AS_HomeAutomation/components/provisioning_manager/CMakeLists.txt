idf_component_register(SRCS "provisioning_manager.c"
                            "blufi_init.c"
                            "blufi_security.c"
                    INCLUDE_DIRS "."
                    REQUIRES log freertos main
                             nvs_flash esp_event esp_netif esp_wifi
                             bt mbedtls
                             json
                             common storage_manager security_manager)
