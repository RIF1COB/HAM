#include "app_core.h"
#include "nvs_flash.h"
#include "esp_err.h"

void app_main(void)
{
    // Initialize NVS (required for Wi-Fi later)
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Initialize application core
    app_core_init();

    // Start tasks
    app_core_start();
}