#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void connectivity_manager_init(void);
void connectivity_manager_task(void *arg);
void connectivity_manager_handle_event(app_event_t *event);

#ifdef __cplusplus
}
#endif

#endif // CONNECTIVITY_MANAGER_H
