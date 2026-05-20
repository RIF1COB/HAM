idf_component_register(SRCS "cloud_client.c"
                    INCLUDE_DIRS "."
                    REQUIRES log freertos main device_control)
