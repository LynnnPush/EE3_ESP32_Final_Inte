#ifndef CLIMATE_CONTROL_H
#define CLIMATE_CONTROL_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "DHT11.h"

// GPIO pins for ESP32-S3-DevKitC-1
#define DHT_GPIO GPIO_NUM_21
#define FAN_GPIO GPIO_NUM_12
#define HEATER_GPIO GPIO_NUM_11

// Default temperature thresholds
#define DEFAULT_TEMP_HIGH 22
#define DEFAULT_TEMP_LOW 20

// Queue to handle sensor data
extern QueueHandle_t sensor_data_queue;

// Timer handles
extern TimerHandle_t sensor_reading_timer;

// Global state variables
extern bool fan_state;
extern bool heater_state;
extern bool fan_override;
extern bool heater_override;
extern DHT11_Data latest_sensor_data;

// Temperature threshold variables (replacing macros with variables)
extern int temp_high;
extern int temp_low;

// Function declarations
void init_control_pins(void);
void control_fan(int temperature);
void control_heater(int temperature);
void set_fan_state(bool state);
void set_heater_state(bool state);
void sensor_reading_timer_callback(TimerHandle_t xTimer);
void read_dht11_task(void *pvParameters);
void process_sensor_data_task(void *pvParameters);
void app_main(void);

// New function to update temperature thresholds
void set_temperature_thresholds(int low, int high);

#endif // CLIMATE_CONTROL_H