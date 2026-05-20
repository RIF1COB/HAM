/*
 * gpio_manager.c
 */


#include "gpio_manager.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "gpio_mgr";

void gpio_manager_init(void)
{
    // Nothing to do globally for now. Per-pin init happens in config_output/pwm_init.
}

int gpio_manager_config_output(int gpio_num, bool initial_level)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed for %d: %d", gpio_num, err);
        return -1;
    }
    gpio_set_level(gpio_num, initial_level ? 1 : 0);
    return 0;
}

int gpio_manager_set_level(int gpio_num, bool level)
{
    esp_err_t err = gpio_set_level(gpio_num, level ? 1 : 0);
    return (err == ESP_OK) ? 0 : -1;
}

int gpio_manager_get_level(int gpio_num, bool *level_out)
{
    int v = gpio_get_level(gpio_num);
    if (level_out) *level_out = (v != 0);
    return 0;
}

// Map PWM channel number to ledc timer/channel configuration. This is minimal
// and assumes timer 0 and high-speed mode. For a production system, make this
// configurable and robust.
int gpio_manager_pwm_init(int pwm_channel, int gpio_num, uint32_t freq_hz)
{
    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = freq_hz,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %d", err);
        return -1;
    }

    ledc_channel_config_t ledc_channel = {
        .channel = (ledc_channel_t)(pwm_channel % LEDC_CHANNEL_MAX),
        .duty = 0,
        .gpio_num = gpio_num,
        .speed_mode = LEDC_HIGH_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = LEDC_TIMER_0,
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %d", err);
        return -1;
    }
    return 0;
}

int gpio_manager_pwm_set_duty(int pwm_channel, uint32_t duty_percent)
{
    if (duty_percent > 100) duty_percent = 100;
    uint32_t max = (1 << LEDC_TIMER_10_BIT) - 1;
    uint32_t duty = (duty_percent * max) / 100;
    esp_err_t err = ledc_set_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)(pwm_channel % LEDC_CHANNEL_MAX), duty);
    if (err == ESP_OK) ledc_update_duty(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)(pwm_channel % LEDC_CHANNEL_MAX));
    return (err == ESP_OK) ? 0 : -1;
}

int gpio_manager_pwm_stop(int pwm_channel)
{
    esp_err_t err = ledc_stop(LEDC_HIGH_SPEED_MODE, (ledc_channel_t)(pwm_channel % LEDC_CHANNEL_MAX), 0);
    return (err == ESP_OK) ? 0 : -1;
}
