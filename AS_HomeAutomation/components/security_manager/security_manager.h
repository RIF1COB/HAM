#ifndef SECURITY_MANAGER_H
#define SECURITY_MANAGER_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void security_manager_init(void);

bool security_manager_store_cert(const char *device_cert, const char *private_key, const char *root_ca);

#ifdef __cplusplus
}
#endif

#endif // SECURITY_MANAGER_H
