#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include "event_bus.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void device_control_init(void);
void device_control_task(void *arg);
void device_control_handle_event(app_event_t *event);

// Handle an incoming cloud JSON packet (raw JSON string)
void device_control_handle_cloud_packet(const char *json_str);

void device_control_set_switch(uint8_t index, bool on);
void device_control_set_fan_speed(uint8_t speed);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_CONTROL_H
