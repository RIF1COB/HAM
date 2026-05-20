/*
 * device_registry.c
 */

#include "device_registry.h"
#include "config_generated.h"
#include <stddef.h>

static const control_mapping_t *g_mappings = NULL;

void device_registry_init(void)
{
    g_mappings = NULL;
}

void device_registry_register_mappings(const control_mapping_t *mappings)
{
    g_mappings = mappings;
}

driver_handle_t device_registry_lookup(logical_control_id_t id)
{
    if (g_mappings == NULL) return NULL;
    const control_mapping_t *m = g_mappings;
    while (m && m->control_id != CONTROL_INVALID) {
        if (m->control_id == id) return m->driver;
        m++;
    }
    return NULL;
}

const char *device_registry_get_config_version(void)
{
    return CFG_VERSION;
}
