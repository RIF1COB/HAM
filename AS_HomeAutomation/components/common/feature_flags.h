/*
 * feature_flags.h
 *
 * Simple feature flag structure to allow SKU-specific feature gating.
 */
#ifndef FEATURE_FLAGS_H
#define FEATURE_FLAGS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool dimming_enabled;
    bool fan_speed_control;
    bool ota_enabled;
    // Extend with more flags as needed
} feature_flags_t;

// Load feature flags from compiled config or runtime source
const feature_flags_t *feature_flags_get(void);

#ifdef __cplusplus
}
#endif

#endif // FEATURE_FLAGS_H
