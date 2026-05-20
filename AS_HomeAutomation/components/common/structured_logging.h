/*
 * structured_logging.h
 *
 * Lightweight structured log tags for observability.
 */
#ifndef STRUCTURED_LOGGING_H
#define STRUCTURED_LOGGING_H

#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_TAG_CFG,
    LOG_TAG_CTRL,
    LOG_TAG_DRV,
    LOG_TAG_AWS,
    LOG_TAG_BLE,
    LOG_TAG_OTA,
    LOG_TAG_FACTORY,
} sh_log_tag_t;

// Map tag to textual prefix used in logs
const char *sh_log_tag_to_str(sh_log_tag_t tag);

// Convenience macro for tagged info logs
#define SH_LOGI(tag, fmt, ...) do { \
    ESP_LOGI(sh_log_tag_to_str(tag), fmt, ##__VA_ARGS__); \
} while(0)

#ifdef __cplusplus
}
#endif

#endif // STRUCTURED_LOGGING_H
