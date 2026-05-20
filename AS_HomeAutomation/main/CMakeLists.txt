set(srcs "app_Main.c"
         "app_core.c"
         "event_bus.c")

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_wifi esp_event esp_netif mqtt esp-tls esp_http_client driver bt esp_timer lwip provisioning_manager connectivity_manager cloud_client device_control storage_manager security_manager)
