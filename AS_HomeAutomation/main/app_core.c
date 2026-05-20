#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "event_bus.h"
#include "app_core.h"
#include <provisioning_manager.h>
#include <connectivity_manager.h>
#include <cloud_client.h>
#include <device_control.h>
#include <storage_manager.h>
#include <security_manager.h>
#include "../components/common/device_registry.h"
#include "../components/common/driver_pool.h"
#include "../components/common/gpio_manager.h"
#include "../components/common/nvs_identity.h"
#include "../components/common/feature_flags.h"
#include "../components/common/structured_logging.h"
#include "../components/common/config_generated.h"
#include <string.h>
#include <stdio.h>

// ---------------- EVENT HANDLER ----------------

static void app_event_handler(app_event_t *event)
{
    printf("[APP_CORE] Event received: %d\n", event->event_id);
}

// ---------------- INIT ----------------

void app_core_init(void)
{
    printf("App core init\n");

    event_bus_init();
    // Initialize common platform subsystems
    gpio_manager_init();
    driver_pool_init();
    device_registry_init();
    // Register generated mappings (may be empty if generator not run yet)
    extern const control_mapping_t VENDOR_DEVICE_MAPPINGS[];
    device_registry_register_mappings(VENDOR_DEVICE_MAPPINGS);

    // Initialize identity storage
    nvs_identity_init();
    // Log and publish compiled configuration version for diagnostics
    const char *cfg_ver = device_registry_get_config_version();
    SH_LOGI(LOG_TAG_CFG, "Config version: %s", cfg_ver);
    char info_json[128];
    snprintf(info_json, sizeof(info_json), "{\"cfg_version\":\"%s\"}", cfg_ver);
    cloud_client_publish_device_info(info_json);
    provisioning_manager_init();
    connectivity_manager_init();
    cloud_client_init();
    device_control_init();
    storage_manager_init();
    security_manager_init();

    // Register handler
    event_bus_register(app_event_handler);
}

// ---------------- START ----------------

void app_core_start(void)
{
    printf("App core start\n");

    // Event dispatcher
    xTaskCreate(event_dispatch_task, "event_task", 4096, NULL, 6, NULL);

    // Core tasks
    xTaskCreate(provisioning_manager_task, "prov_task", 4096, NULL, 5, NULL);
    xTaskCreate(connectivity_manager_task, "wifi_task", 4096, NULL, 5, NULL);
    xTaskCreate(cloud_client_task, "mqtt_task", 4096, NULL, 5, NULL);
    xTaskCreate(device_control_task, "device_task", 2048, NULL, 5, NULL);
}