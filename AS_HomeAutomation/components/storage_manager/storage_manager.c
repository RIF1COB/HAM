#include "storage_manager.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "storage_manager";

void storage_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        ret = nvs_flash_init();
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "storage manager initialized");
    } else {
        ESP_LOGE(TAG, "storage manager init failed: %d", ret);
    }
}

bool storage_manager_save_wifi_credentials(const char *ssid, const char *password)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open wifi failed: %d", err);
        return false;
    }

    err = nvs_set_str(handle, "ssid", ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "password", password);
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err == ESP_OK;
}

bool storage_manager_save_aws_config(const char *endpoint, const char *cert)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("aws", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open aws failed: %d", err);
        return false;
    }

    err = nvs_set_str(handle, "endpoint", endpoint);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "cert", cert);
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err == ESP_OK;
}
