#include "device_control.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "device_registry.h"
#include "driver_pool.h"
#include <string.h>

static const char *TAG = "device_control";

void device_control_init(void)
{
    ESP_LOGI(TAG, "device control initialized");
}

void device_control_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "device control task running2111111111111111111111111");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void device_control_handle_event(app_event_t *event)
{
    ESP_LOGI(TAG, "device control event received: %u", event->event_id);
}

void device_control_set_switch(uint8_t index, bool on)
{
    ESP_LOGI(TAG, "set switch %u %s", index, on ? "ON" : "OFF");

    // Map numeric index to logical control id. Adjust mapping as needed.
    logical_control_id_t cid = CONTROL_INVALID;
    switch (index) {
        case 1: cid = CONTROL_LIGHT_1; break;
        case 2: cid = CONTROL_LIGHT_2; break;
        case 3: cid = CONTROL_LIGHT_3; break;
        case 4: cid = CONTROL_FAN_1; break;
        case 5: cid = CONTROL_FAN_2; break;
        default: cid = CONTROL_INVALID; break;
    }

    if (cid == CONTROL_INVALID) {
        ESP_LOGW(TAG, "Unknown switch index %u", index);
        return;
    }

    driver_handle_t drv = device_registry_lookup(cid);
    if (!drv) {
        ESP_LOGW(TAG, "No driver registered for control index %u", index);
        return;
    }

    int cmd = on ? 1 : 0; // 1=on,0=off
    int rc = driver_execute((generic_driver_t *)drv, cmd, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "driver_execute failed for index %u", index);
    }
}

void device_control_set_fan_speed(uint8_t speed)
{
    ESP_LOGI(TAG, "set fan speed %u", speed);

    // For this demo, control FAN_1
    driver_handle_t drv = device_registry_lookup(CONTROL_FAN_1);
    if (!drv) {
        ESP_LOGW(TAG, "No driver registered for fan");
        return;
    }
    int rc = driver_execute((generic_driver_t *)drv, 2, &speed);
    if (rc != 0) {
        ESP_LOGE(TAG, "driver_execute failed for fan speed");
    }
}

// Weak fallback: simple JSON parser to support simulated cloud packets
// If a stronger implementation exists (e.g., in cloud_command.c), it will override this.
__attribute__((weak)) void device_control_handle_cloud_packet(const char *json_str)
{
    if (!json_str) return;

    // Very small and tolerant parse: look for control name and value
    if (strstr(json_str, "LIGHT_1") != NULL) {
        if (strstr(json_str, "\"value\":1") != NULL) {
            device_control_set_switch(1, true);
        } else {
            device_control_set_switch(1, false);
        }
        return;
    }

    if (strstr(json_str, "FAN_1") != NULL) {
        // extract numeric speed if present
        const char *p = strstr(json_str, "\"speed\":");
        if (p) {
            int sp = atoi(p + strlen("\"speed\":"));
            device_control_set_fan_speed((uint8_t)sp);
        }
        return;
    }
}
