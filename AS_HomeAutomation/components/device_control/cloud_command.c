/*
 * cloud_command.c
 *
 * Parse incoming cloud JSON packets and dispatch to device control drivers.
 */

#include "device_control.h"
#include "device_registry.h"
#include "driver_pool.h"
#include "esp_log.h"
#include <string.h>
#include "cJSON.h"

static const char *TAG = "cloud_cmd";

static logical_control_id_t control_from_string(const char *s)
{
    if (!s) return CONTROL_INVALID;
    if (strcmp(s, "LIGHT_1") == 0) return CONTROL_LIGHT_1;
    if (strcmp(s, "LIGHT_2") == 0) return CONTROL_LIGHT_2;
    if (strcmp(s, "LIGHT_3") == 0) return CONTROL_LIGHT_3;
    if (strcmp(s, "FAN_1") == 0) return CONTROL_FAN_1;
    if (strcmp(s, "FAN_2") == 0) return CONTROL_FAN_2;
    return CONTROL_INVALID;
}

void device_control_handle_cloud_packet(const char *json_str)
{
    if (!json_str) return;

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        ESP_LOGE(TAG, "Invalid JSON packet");
        return;
    }

    cJSON *msg_type = cJSON_GetObjectItem(root, "msg_type");
    if (!msg_type || !cJSON_IsString(msg_type) || strcmp(msg_type->valuestring, "CMD") != 0) {
        cJSON_Delete(root);
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
    cJSON *sub_cmd = cJSON_GetObjectItem(root, "sub_cmd");
    cJSON *payload = cJSON_GetObjectItem(root, "payload");

    if (!cmd || !cJSON_IsString(cmd) || !sub_cmd || !cJSON_IsString(sub_cmd) || !payload) {
        ESP_LOGE(TAG, "Malformed envelope");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(cmd->valuestring, "DEVICE_CONTROL") == 0) {
        if (strcmp(sub_cmd->valuestring, "SET_SWITCH") == 0) {
            cJSON *control = cJSON_GetObjectItem(payload, "control");
            cJSON *value = cJSON_GetObjectItem(payload, "value");
            if (!control || !cJSON_IsString(control) || !value || !cJSON_IsNumber(value)) {
                ESP_LOGE(TAG, "Missing payload fields for SET_SWITCH");
                cJSON_Delete(root);
                return;
            }

            logical_control_id_t cid = control_from_string(control->valuestring);
            if (cid == CONTROL_INVALID) {
                ESP_LOGE(TAG, "Unknown control '%s'", control->valuestring);
                cJSON_Delete(root);
                return;
            }

            driver_handle_t drv = device_registry_lookup(cid);
            if (!drv) {
                ESP_LOGE(TAG, "No driver registered for %s", control->valuestring);
                cJSON_Delete(root);
                return;
            }

            int on = (value->valueint != 0) ? 1 : 0;
            int rc = driver_execute((generic_driver_t *)drv, on ? 1 : 0, NULL);
            if (rc == 0) {
                ESP_LOGI(TAG, "SET_SWITCH %s -> %d executed", control->valuestring, on);
            } else {
                ESP_LOGE(TAG, "Driver execution failed for %s", control->valuestring);
            }

        } else if (strcmp(sub_cmd->valuestring, "SET_FAN_SPEED") == 0) {
            cJSON *control = cJSON_GetObjectItem(payload, "control");
            cJSON *speed = cJSON_GetObjectItem(payload, "speed");
            if (!control || !cJSON_IsString(control) || !speed || !cJSON_IsNumber(speed)) {
                ESP_LOGE(TAG, "Missing payload fields for SET_FAN_SPEED");
                cJSON_Delete(root);
                return;
            }

            logical_control_id_t cid = control_from_string(control->valuestring);
            if (cid == CONTROL_INVALID) {
                ESP_LOGE(TAG, "Unknown control '%s'", control->valuestring);
                cJSON_Delete(root);
                return;
            }

            driver_handle_t drv = device_registry_lookup(cid);
            if (!drv) {
                ESP_LOGE(TAG, "No driver registered for %s", control->valuestring);
                cJSON_Delete(root);
                return;
            }

            int sp = speed->valueint;
            // Use driver_execute with params pointer to speed (driver should interpret)
            int rc = driver_execute((generic_driver_t *)drv, 2, &sp);
            if (rc == 0) {
                ESP_LOGI(TAG, "SET_FAN_SPEED %s -> %d executed", control->valuestring, sp);
            } else {
                ESP_LOGE(TAG, "Driver execution failed for %s", control->valuestring);
            }

        } else {
            ESP_LOGW(TAG, "Unhandled DEVICE_CONTROL sub_cmd: %s", sub_cmd->valuestring);
        }
    }

    cJSON_Delete(root);
}
