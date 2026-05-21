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

// Provisioning-flag helpers. The flag lives inside the identity blob but
// can be flipped independently of the write-once identity fields (vendor_id,
// device_id) which remain immutable once written.
// Returns true iff identity is written AND its provisioned field is non-zero.
bool nvs_identity_is_provisioned(void);

// Set/clear the provisioned flag in the identity blob. If no identity has
// been written yet, a default identity (vendor_id=0, device_id=0) is created
// so subsequent factory programming can still write the real IDs (the
// write-once compare ignores the provisioned byte).
esp_err_t nvs_identity_set_provisioned(bool provisioned);

#ifdef __cplusplus
}
#endif

#endif // NVS_IDENTITY_H
