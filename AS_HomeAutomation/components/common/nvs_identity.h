/*
 * nvs_identity.h
 *
 * Device identity stored in NVS. Identity fields are intended to be write-once
 * in factory and must not contain hardware mapping information.
 */
#ifndef NVS_IDENTITY_H
#define NVS_IDENTITY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t provisioned; // 0 = false, 1 = true
} device_identity_t;

// Initialize NVS identity module (calls nvs_flash_init internally if needed)
esp_err_t nvs_identity_init(void);

// Write identity only if not already present. Returns ESP_OK if present and matching.
esp_err_t nvs_identity_write_once(const device_identity_t *id);

// Read identity. Returns ESP_OK if present.
esp_err_t nvs_identity_read(device_identity_t *out);

// Returns true if identity has been written to NVS
bool nvs_identity_is_written(void);

#ifdef __cplusplus
}
#endif

#endif // NVS_IDENTITY_H
