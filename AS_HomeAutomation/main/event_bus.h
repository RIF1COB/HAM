#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>

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