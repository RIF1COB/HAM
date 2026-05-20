#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void storage_manager_init(void);

bool storage_manager_save_wifi_credentials(const char *ssid, const char *password);
bool storage_manager_save_aws_config(const char *endpoint, const char *cert);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_MANAGER_H
