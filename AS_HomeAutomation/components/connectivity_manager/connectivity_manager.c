#include "connectivity_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "connectivity_mgr";

void connectivity_manager_init(void)
{
    ESP_LOGI(TAG, "connectivity manager initialized");
}

void connectivity_manager_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "connectivity task running################################");
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

void connectivity_manager_handle_event(app_event_t *event)
{
    ESP_LOGI(TAG, "connectivity event received: %u", event->event_id);
}
