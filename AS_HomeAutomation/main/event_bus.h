#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>

// Well-known event IDs published on the application event bus.
// Numeric values are stable across the project; do not renumber.
typedef enum {
    EVT_NONE             = 0,
    EVT_WIFI_CONFIGURED  = 1,  // Provisioning completed; creds in NVS namespace "wifi"
    EVT_NET_READY        = 2,  // STA got IP; cloud layer may connect
    EVT_FACTORY_RESET    = 3,  // Request to clear wifi/aws/certs and reboot to BLE prov
} app_event_id_t;

// Basic event structure
typedef struct {
    uint16_t event_id;
    void *data;
} app_event_t;

// Handler type
typedef void (*event_handler_t)(app_event_t *event);

// APIs
void event_bus_init(void);
void event_bus_post(app_event_t *event);
void event_bus_register(event_handler_t handler);

// Dispatcher task
void event_dispatch_task(void *arg);

#endif