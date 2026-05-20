idf_component_register(SRCS "device_control.c" "cloud_command.c"
                    INCLUDE_DIRS "."
                    REQUIRES log freertos main common json)
