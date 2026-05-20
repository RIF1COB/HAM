/*
 * device_registry.h
 *
 * Central registry for logical controls -> driver mappings.
 * Generated configuration (from Excel) will populate mapping tables
 * which are registered at startup via `device_registry_register_mappings()`.
 */
#ifndef DEVICE_REGISTRY_H
#define DEVICE_REGISTRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stable logical control identifiers (extend as needed)
typedef enum {
    CONTROL_INVALID = 0,
    CONTROL_LIGHT_1,
    CONTROL_LIGHT_2,
    CONTROL_LIGHT_3,
    CONTROL_FAN_1,
    CONTROL_FAN_2,
    // Add new logical controls here. Do NOT remove entries once released.
} logical_control_id_t;

// Opaque pointer to a driver instance (defined by driver pool)
typedef void *driver_handle_t;

// Mapping entry produced by generated configuration
typedef struct {
    logical_control_id_t control_id;
    driver_handle_t driver;
} control_mapping_t;

// Initialize device registry (call early in boot)
void device_registry_init(void);

// Register mapping list (terminated by entry with control_id == CONTROL_INVALID)
void device_registry_register_mappings(const control_mapping_t *mappings);

// Lookup driver for a logical control id. Returns NULL if not found.
driver_handle_t device_registry_lookup(logical_control_id_t id);

// Get compiled configuration version string (from generated header)
const char *device_registry_get_config_version(void);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_REGISTRY_H
