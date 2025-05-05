#ifndef DHT11_H
#define DHT11_H

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

typedef struct {
    int temperature;  // Temperature in Celsius
    int humidity;     // Humidity percentage
} DHT11_Data;

/**
 * @brief Initialize DHT11 sensor
 * @param gpio GPIO pin connected to the DHT11 sensor
 */
void DHT11_Init(gpio_num_t gpio);

/**
 * @brief Read temperature and humidity from DHT11 sensor
 * @return DHT11_Data structure containing temperature and humidity values
 *         Returns -1 for both values if reading fails
 */
DHT11_Data DHT11_Read();

/**
 * @brief Task to read DHT11 sensor and send data to queue
 * @param pvParameters Task parameters (unused)
 */
void read_dht11_task(void *pvParameters);

/**
 * @brief Timer callback function to trigger DHT11 reading
 * @param xTimer Timer handle
 */
void sensor_reading_timer_callback(TimerHandle_t xTimer);

/**
 * @brief Task to process sensor data and control devices
 * @param pvParameters Task parameters (unused)
 */
void process_sensor_data_task(void *pvParameters);

#endif // DHT11_H