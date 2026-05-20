/*
 * feature_flags.c
 */

#include "feature_flags.h"

static feature_flags_t s_flags = {
    .dimming_enabled = false,
    .fan_speed_control = true,
    .ota_enabled = true,
};

const feature_flags_t *feature_flags_get(void)
{
    return &s_flags;
}
