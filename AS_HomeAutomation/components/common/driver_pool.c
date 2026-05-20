/*
 * driver_pool.c
 */

#include "driver_pool.h"
#include "gpio_manager.h"

// Sample driver instances for example mappings. These provide symbols
// referenced by the generated `config_generated.c`. Adjust GPIO numbers
// and PWM params to match your hardware.
static switch_driver_t SWITCH1_impl = { .gpio_num = 2, .active_high = true };
static switch_driver_t SWITCH2_impl = { .gpio_num = 4, .active_high = true };
static fan_driver_t FAN1_impl = { .pwm_channel = 0, .pwm_gpio = 5, .pwm_freq_hz = 5000 };

// Exposed generic driver objects (symbols referenced by generator)
generic_driver_t SWITCH1_driver = { .type = DRIVER_TYPE_SWITCH, .drv = &SWITCH1_impl };
generic_driver_t SWITCH2_driver = { .type = DRIVER_TYPE_SWITCH, .drv = &SWITCH2_impl };
generic_driver_t FAN1_driver = { .type = DRIVER_TYPE_FAN, .drv = &FAN1_impl };

void driver_pool_init(void)
{
    // Initialize underlying GPIO/PWM subsystems
    gpio_manager_init();
}

int driver_execute(generic_driver_t *driver, int command, void *params)
{
    if (!driver) return -1;
    switch (driver->type) {
        case DRIVER_TYPE_SWITCH: {
            switch_driver_t *s = (switch_driver_t *)driver->drv;
            if (!s) return -1;
            // command: 0 = off, 1 = on
            if (command == 0) {
                gpio_manager_set_level(s->gpio_num, !s->active_high ? 0 : 0);
            } else {
                gpio_manager_set_level(s->gpio_num, s->active_high ? 1 : 1);
            }
            return 0;
        }
        case DRIVER_TYPE_FAN: {
            fan_driver_t *f = (fan_driver_t *)driver->drv;
            if (!f) return -1;
            // params expected to be pointer to int duty percent
            if (params) {
                int duty = *((int *)params);
                gpio_manager_pwm_set_duty(f->pwm_channel, duty);
                return 0;
            }
            return -1;
        }
        default:
            return -1;
    }
}
