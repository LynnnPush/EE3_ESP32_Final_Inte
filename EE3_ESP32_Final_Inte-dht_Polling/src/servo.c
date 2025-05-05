#include "servo.h"
#include "driver/ledc.h"
#include "esp_err.h"

static uint32_t servo_min_pulse = DEFAULT_SERVO_MIN_PULSE;
static uint32_t servo_max_pulse = DEFAULT_SERVO_MAX_PULSE;
static uint32_t servo_freq      = DEFAULT_SERVO_FREQ;
static ledc_timer_bit_t servo_resolution = DEFAULT_SERVO_RESOLUTION;

void servo_init(gpio_num_t gpio_pin,
                ledc_channel_t ledc_channel,
                ledc_timer_t ledc_timer_num,
                ledc_mode_t ledc_mode)
{
    // Configure LEDC timer
    ledc_timer_config_t timer_cfg = {
        .duty_resolution = servo_resolution,
        .freq_hz         = servo_freq,
        .speed_mode      = ledc_mode,
        .timer_num       = ledc_timer_num,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK( ledc_timer_config(&timer_cfg) );

    // Configure LEDC channel
    ledc_channel_config_t channel_cfg = {
        .channel    = ledc_channel,
        .duty       = 0,
        .gpio_num   = gpio_pin,
        .speed_mode = ledc_mode,
        .hpoint     = 0,
        .timer_sel  = ledc_timer_num
    };
    ESP_ERROR_CHECK( ledc_channel_config(&channel_cfg) );
}

void servo_move(int angle,
                ledc_channel_t ledc_channel,
                ledc_mode_t ledc_mode)
{
    if (angle < -90) angle = -90;
    if (angle >  90) angle =  90;

    uint32_t pulse_width = servo_min_pulse +
        ((uint32_t)(angle + 90) * (servo_max_pulse - servo_min_pulse)) / 180;
    uint32_t duty = (pulse_width * ((1 << servo_resolution) - 1)) 
                    / (1000000 / servo_freq);

    ESP_ERROR_CHECK( ledc_set_duty(ledc_mode, ledc_channel, duty) );
    ESP_ERROR_CHECK( ledc_update_duty(ledc_mode, ledc_channel) );
}

void servo_set_pulse_width(uint32_t pulse_width_us,
                           ledc_channel_t ledc_channel,
                           ledc_mode_t ledc_mode)
{
    uint32_t duty = (pulse_width_us * ((1 << servo_resolution) - 1)) 
                    / (1000000 / servo_freq);
    ESP_ERROR_CHECK( ledc_set_duty(ledc_mode, ledc_channel, duty) );
    ESP_ERROR_CHECK( ledc_update_duty(ledc_mode, ledc_channel) );
}
