#include "provisioning_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "provisioning_mgr";

void provisioning_manager_init(void)
{
    ESP_LOGI(TAG, "provisioning manager initialized");
}

void provisioning_manager_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "provisioning task running");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void provisioning_manager_handle_event(app_event_t *event)
{
    ESP_LOGI(TAG, "provisioning event received: %u", event->event_id);
}
