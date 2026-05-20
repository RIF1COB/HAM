#include "security_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "security_manager";

void security_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "security manager initialized");
    } else {
        ESP_LOGE(TAG, "security manager init failed: %d", ret);
    }
}

bool security_manager_store_cert(const char *device_cert, const char *private_key, const char *root_ca)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("certs", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open certs failed: %d", err);
        return false;
    }

    err = nvs_set_str(handle, "device_cert", device_cert);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "private_key", private_key);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "root_ca", root_ca);
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err == ESP_OK;
}
