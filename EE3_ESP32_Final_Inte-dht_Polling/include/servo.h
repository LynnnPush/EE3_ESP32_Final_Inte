#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include "driver/gpio.h"

// Servo configuration defaults
#define DEFAULT_SERVO_FREQ      50              // 50 Hz PWM frequency (20ms period)
#define DEFAULT_SERVO_MIN_PULSE 500             // Minimum pulse width (µs)
#define DEFAULT_SERVO_MAX_PULSE 2500            // Maximum pulse width (µs)
#define DEFAULT_SERVO_RESOLUTION LEDC_TIMER_14_BIT  // 14-bit resolution

/**
 * @brief Initialize servo motor with default PWM configuration
 * @param gpio_pin GPIO pin connected to the servo
 * @param ledc_channel LEDC channel to use
 * @param ledc_timer LEDC timer to use
 * @param ledc_mode LEDC speed mode
 */
void servo_init(gpio_num_t gpio_pin, ledc_channel_t ledc_channel, 
                ledc_timer_t ledc_timer, ledc_mode_t ledc_mode);

/**
 * @brief Initialize servo motor with custom PWM configuration
 * @param gpio_pin GPIO pin connected to the servo
 * @param ledc_channel LEDC channel to use
 * @param ledc_timer LEDC timer to use
 * @param ledc_mode LEDC speed mode
 * @param freq_hz PWM frequency in Hz
 * @param min_pulse_width_us Minimum pulse width in microseconds
 * @param max_pulse_width_us Maximum pulse width in microseconds
 * @param resolution PWM resolution
 */
void servo_init_custom(gpio_num_t gpio_pin, ledc_channel_t ledc_channel, 
                       ledc_timer_t ledc_timer, ledc_mode_t ledc_mode,
                       uint32_t freq_hz, uint32_t min_pulse_width_us, 
                       uint32_t max_pulse_width_us, ledc_timer_bit_t resolution);

/**
 * @brief Move servo to specified angle
 * @param angle Angle in degrees (-90 to 90)
 * @param ledc_channel LEDC channel configured for this servo
 * @param ledc_mode LEDC speed mode
 */
void servo_move(int angle, ledc_channel_t ledc_channel, ledc_mode_t ledc_mode);

/**
 * @brief Move servo to specified angle with custom range
 * @param angle Angle in degrees (min_angle to max_angle)
 * @param min_angle Minimum angle in degrees
 * @param max_angle Maximum angle in degrees
 * @param ledc_channel LEDC channel configured for this servo
 * @param ledc_mode LEDC speed mode
 */
void servo_move_range(int angle, int min_angle, int max_angle, 
                     ledc_channel_t ledc_channel, ledc_mode_t ledc_mode);

/**
 * @brief Set servo position directly using pulse width
 * @param pulse_width_us Pulse width in microseconds
 * @param ledc_channel LEDC channel configured for this servo
 * @param ledc_mode LEDC speed mode
 */
void servo_set_pulse_width(uint32_t pulse_width_us, ledc_channel_t ledc_channel, 
                          ledc_mode_t ledc_mode);

#endif // SERVO_CONTROL_H