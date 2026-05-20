#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "event_bus.h"

#define EVENT_QUEUE_SIZE 10

static QueueHandle_t event_queue = NULL;
static event_handler_t registered_handler = NULL;

void event_bus_init(void)
{
    event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(app_event_t));

    if (event_queue == NULL) {
        printf("Failed to create event queue\n");
    } else {
        printf("Event bus initialized\n");
    }
}

void event_bus_post(app_event_t *event)
{
    if (event_queue == NULL) return;

    xQueueSend(event_queue, event, portMAX_DELAY);
}

void event_bus_register(event_handler_t handler)
{
    registered_handler = handler;
}

void event_dispatch_task(void *arg)
{
    app_event_t event;

    while (1) {
        if (xQueueReceive(event_queue, &event, portMAX_DELAY)) {

            printf("[EVENT_BUS] Dispatching event: %d\n", event.event_id);

            if (registered_handler) {
                registered_handler(&event);
            }
        }
    }
}