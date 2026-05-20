/*
 * gpio_manager.h
 *
 * Hardware abstraction for GPIO and PWM to make drivers testable and mockable.
 */
#ifndef GPIO_MANAGER_H
#define GPIO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize GPIO manager (configure peripheral drivers)
void gpio_manager_init(void);

// Configure a pin as output
int gpio_manager_config_output(int gpio_num, bool initial_level);

// Set output level (0/1)
int gpio_manager_set_level(int gpio_num, bool level);

// Get input level
int gpio_manager_get_level(int gpio_num, bool *level_out);

// PWM API (simplified)
int gpio_manager_pwm_init(int pwm_channel, int gpio_num, uint32_t freq_hz);
int gpio_manager_pwm_set_duty(int pwm_channel, uint32_t duty_percent);
int gpio_manager_pwm_stop(int pwm_channel);

#ifdef __cplusplus
}
#endif

#endif // GPIO_MANAGER_H
