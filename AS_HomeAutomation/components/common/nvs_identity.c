/*
 * nvs_identity.c
 */

#include "nvs_identity.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *NVS_NAMESPACE = "identity";
static const char *NVS_KEY = "identity_blob";

esp_err_t nvs_identity_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Erase and retry
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    return err;
}

esp_err_t nvs_identity_write_once(const device_identity_t *id)
{
    if (!id) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    // Check if already written. The write-once policy applies only to the
    // immutable identity fields (vendor_id, device_id); the `provisioned`
    // byte is mutable and is handled by nvs_identity_set_provisioned().
    size_t required = 0;
    err = nvs_get_blob(h, NVS_KEY, NULL, &required);
    if (err == ESP_OK && required == sizeof(device_identity_t)) {
        device_identity_t existing;
        err = nvs_get_blob(h, NVS_KEY, &existing, &required);
        if (err == ESP_OK) {
            if (existing.vendor_id == id->vendor_id &&
                existing.device_id == id->device_id) {
                nvs_close(h);
                return ESP_OK; // immutable fields match
            } else {
                nvs_close(h);
                return ESP_ERR_INVALID_STATE; // cannot overwrite different identity
            }
        }
    }

    // Write blob
    err = nvs_set_blob(h, NVS_KEY, id, sizeof(device_identity_t));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t nvs_identity_read(device_identity_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t required = sizeof(device_identity_t);
    err = nvs_get_blob(h, NVS_KEY, out, &required);
    nvs_close(h);
    return err;
}

bool nvs_identity_is_written(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;
    size_t required = 0;
    esp_err_t err = nvs_get_blob(h, NVS_KEY, NULL, &required);
    nvs_close(h);
    return (err == ESP_OK && required == sizeof(device_identity_t));
}

bool nvs_identity_is_provisioned(void)
{
    device_identity_t id;
    if (nvs_identity_read(&id) != ESP_OK) return false;
    return id.provisioned != 0;
}

esp_err_t nvs_identity_set_provisioned(bool provisioned)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    device_identity_t id = { .vendor_id = 0, .device_id = 0, .provisioned = 0 };
    size_t required = sizeof(id);
    esp_err_t rerr = nvs_get_blob(h, NVS_KEY, &id, &required);
    // If blob missing or wrong size, fall through and create a default one.
    (void)rerr;

    id.provisioned = provisioned ? 1 : 0;
    err = nvs_set_blob(h, NVS_KEY, &id, sizeof(id));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
