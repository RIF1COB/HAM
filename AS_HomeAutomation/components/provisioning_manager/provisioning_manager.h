#ifndef PROVISIONING_MANAGER_H
#define PROVISIONING_MANAGER_H

#include <stdbool.h>
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

void provisioning_manager_init(void);
void provisioning_manager_task(void *arg);
void provisioning_manager_handle_event(app_event_t *event);

// Returns true when the provisioning manager has finished BLE provisioning
// (or skipped it because the device is already provisioned) and Wi-Fi
// credentials are available in NVS.
bool provisioning_manager_is_complete(void);

// Trigger a factory reset: erase the wifi/aws/certs NVS namespaces (identity
// is preserved per FR-PROV-8), publish EVT_FACTORY_RESET, and reboot back
// into the BLE provisioning flow on next boot.
void provisioning_manager_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif // PROVISIONING_MANAGER_H
