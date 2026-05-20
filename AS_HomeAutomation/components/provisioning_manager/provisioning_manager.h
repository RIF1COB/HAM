#ifndef PROVISIONING_MANAGER_H
#define PROVISIONING_MANAGER_H

#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void provisioning_manager_init(void);
void provisioning_manager_task(void *arg);
void provisioning_manager_handle_event(app_event_t *event);

#ifdef __cplusplus
}
#endif

#endif // PROVISIONING_MANAGER_H
