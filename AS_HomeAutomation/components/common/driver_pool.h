/*
 * driver_pool.h
 *
 * Definitions for the fixed driver pool representing hardware capabilities.
 * Drivers are hardware-only and must not contain vendor/product logic.
 */
#ifndef DRIVER_POOL_H
#define DRIVER_POOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DRIVER_TYPE_INVALID = 0,
    DRIVER_TYPE_SWITCH,
    DRIVER_TYPE_FAN,
    // Future types: DIMMER, SENSOR, ENERGY_METER, etc.
} driver_type_t;

// Switch driver: simple GPIO on/off
typedef struct {
    int gpio_num;
    bool active_high;
} switch_driver_t;

// Fan driver: PWM-based control
typedef struct {
    int pwm_channel;
    int pwm_gpio;
    uint32_t pwm_freq_hz;
} fan_driver_t;

// Generic driver handle: callers should cast based on driver type
typedef struct {
    driver_type_t type;
    void *drv; // points to driver-specific struct (switch_driver_t / fan_driver_t)
} generic_driver_t;

// Initialize all low-level drivers (GPIO, PWM setups)
void driver_pool_init(void);

// Execute a command on a driver. command and params are driver-specific.
// Returns 0 on success, negative on error.
int driver_execute(generic_driver_t *driver, int command, void *params);

// Sample driver symbols exposed for generated mappings.
// These are provided as examples and can be replaced with board-specific drivers.
extern generic_driver_t SWITCH1_driver;
extern generic_driver_t SWITCH2_driver;
extern generic_driver_t FAN1_driver;

#ifdef __cplusplus
}
#endif

#endif // DRIVER_POOL_H
