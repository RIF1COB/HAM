#ifndef CLOUD_CLIENT_H
#define CLOUD_CLIENT_H

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void cloud_client_init(void);
void cloud_client_task(void *arg);
void cloud_client_handle_event(app_event_t *event);

// Publish device info (firmware/config/version/telemetry) to cloud
// Implementations should be safe to call early and may buffer until connection
void cloud_client_publish_device_info(const char *info_json);

// Test helper: simulate receiving a JSON packet from BLE (or other local source)
void cloud_client_simulate_ble_receive(const char *json_str);

#ifdef __cplusplus
}
#endif

#endif // CLOUD_CLIENT_H
