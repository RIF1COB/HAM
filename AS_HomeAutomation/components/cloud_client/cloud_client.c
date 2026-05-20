#include "cloud_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "device_control.h"

static const char *TAG = "cloud_client";

void cloud_client_init(void)
{
    ESP_LOGI(TAG, "cloud client initialized");

    // Start a small simulated AWS packet generator for testing the data format
    // This sends commands to the device control layer as if received from AWS.
    extern void aws_simulation_task(void *arg);
    xTaskCreate(aws_simulation_task, "aws_sim_task", 4096, NULL, 5, NULL);
}

// Simulation task implementation
void aws_simulation_task(void *arg)
{
    (void)arg;
    // Small startup delay
    vTaskDelay(pdMS_TO_TICKS(5000));

    // Example packet: turn LIGHT_1 ON
    const char *pkt_on = "{\"protocol_ver\":1,\"msg_type\":\"CMD\",\"cmd\":\"DEVICE_CONTROL\",\"sub_cmd\":\"SET_SWITCH\",\"msg_id\":1001,\"device_id\":\"DEV_001\",\"payload\":{\"control\":\"LIGHT_1\",\"value\":1},\"timestamp\":1715352000}";
    ESP_LOGI(TAG, "Simulating AWS packet: TURN ON LIGHT_1");
    device_control_handle_cloud_packet(pkt_on);

    // Wait 30 seconds
    vTaskDelay(pdMS_TO_TICKS(30000));

    // Turn LIGHT_1 OFF
    const char *pkt_off = "{\"protocol_ver\":1,\"msg_type\":\"CMD\",\"cmd\":\"DEVICE_CONTROL\",\"sub_cmd\":\"SET_SWITCH\",\"msg_id\":1002,\"device_id\":\"DEV_001\",\"payload\":{\"control\":\"LIGHT_1\",\"value\":0},\"timestamp\":1715352030}";
    ESP_LOGI(TAG, "Simulating AWS packet: TURN OFF LIGHT_1");
    device_control_handle_cloud_packet(pkt_off);

    // Wait another 30 seconds
    vTaskDelay(pdMS_TO_TICKS(30000));

    // Blink LIGHT_1 for a few cycles by toggling
    ESP_LOGI(TAG, "Simulating AWS packet: BLINK LIGHT_1 (via toggles)");
    for (int i = 0; i < 6; ++i) {
        const char *pkt_blink_on = "{\"protocol_ver\":1,\"msg_type\":\"CMD\",\"cmd\":\"DEVICE_CONTROL\",\"sub_cmd\":\"SET_SWITCH\",\"msg_id\":2000,\"device_id\":\"DEV_001\",\"payload\":{\"control\":\"LIGHT_1\",\"value\":1},\"timestamp\":1715352060}";
        device_control_handle_cloud_packet(pkt_blink_on);
        vTaskDelay(pdMS_TO_TICKS(500));
        const char *pkt_blink_off = "{\"protocol_ver\":1,\"msg_type\":\"CMD\",\"cmd\":\"DEVICE_CONTROL\",\"sub_cmd\":\"SET_SWITCH\",\"msg_id\":2001,\"device_id\":\"DEV_001\",\"payload\":{\"control\":\"LIGHT_1\",\"value\":0},\"timestamp\":1715352060}";
        device_control_handle_cloud_packet(pkt_blink_off);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // End simulation task
    vTaskDelete(NULL);
}

void cloud_client_task(void *arg)
{
    while (1) {
        ESP_LOGI(TAG, "cloud client task running");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void cloud_client_handle_event(app_event_t *event)
{
    ESP_LOGI(TAG, "cloud event received: %u", event->event_id);
}

void cloud_client_publish_device_info(const char *info_json)
{
    if (info_json == NULL) {
        ESP_LOGW(TAG, "publish_device_info called with NULL");
        return;
    }
    // Placeholder: actual implementation should publish to AWS IoT topics
    ESP_LOGI(TAG, "Device info: %s", info_json);
}

// Simulate receiving a JSON packet from BLE/mobile app; routes to same handler
void cloud_client_simulate_ble_receive(const char *json_str)
{
    if (!json_str) return;
    ESP_LOGI(TAG, "Simulated BLE packet received");
    device_control_handle_cloud_packet(json_str);
}
