#include "climate_control.h"

// Global variables
QueueHandle_t sensor_data_queue;
TimerHandle_t sensor_reading_timer;
bool fan_state = false;
bool heater_state = false;
bool fan_override = false;
bool heater_override = false;
DHT11_Data latest_sensor_data = {.temperature = 0, .humidity = 0};

// Initialize temperature thresholds with defaults
int temp_high = DEFAULT_TEMP_HIGH;
int temp_low = DEFAULT_TEMP_LOW;

static const char *TAG = "climate";

// Initialize GPIO pins for fan and heater
void init_control_pins() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << FAN_GPIO) | (1ULL << HEATER_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
    // Initial state: both off
    gpio_set_level(FAN_GPIO, 0);
    gpio_set_level(HEATER_GPIO, 0);
}

// Set fan state directly (for manual control)
void set_fan_state(bool state) {
    fan_state = state;
    gpio_set_level(FAN_GPIO, fan_state ? 1 : 0);
}

// Set heater state directly (for manual control)
void set_heater_state(bool state) {
    heater_state = state;
    gpio_set_level(HEATER_GPIO, heater_state ? 1 : 0);
}

// Function to update temperature thresholds
void set_temperature_thresholds(int low, int high) {
    // Validate input
    if (low < high) {
        temp_low = low;
        temp_high = high;
        ESP_LOGI(TAG, "Temperature thresholds updated - Low: %d°C, High: %d°C", temp_low, temp_high);
    } else {
        ESP_LOGW(TAG, "Invalid thresholds (low must be less than high) - Low: %d°C, High: %d°C", low, high);
    }
}

// Control fan based on temperature
void control_fan(int temperature) {
    if (fan_override) {
        // Skip auto control if override active
        return;
    }
    
    bool new_state = (temperature > temp_high);
    
    // Only update if state changed
    if (new_state != fan_state) {
        fan_state = new_state;
        gpio_set_level(FAN_GPIO, fan_state ? 1 : 0);
    }
}

// Control heater based on temperature
void control_heater(int temperature) {
    if (heater_override) {
        // Skip auto control if override active
        return;
    }
    
    bool new_state = (temperature < temp_low);
    
    // Only update if state changed
    if (new_state != heater_state) {
        heater_state = new_state;
        gpio_set_level(HEATER_GPIO, heater_state ? 1 : 0);
    }
}

// Timer callback function to read DHT11 sensor
void sensor_reading_timer_callback(TimerHandle_t xTimer) {
    // Start sensor reading in a separate task to avoid blocking
    xTaskCreate(read_dht11_task, "read_dht11_task", 4096, NULL, 5, NULL);
}

// Task to read DHT11 sensor and send data to queue
void read_dht11_task(void *pvParameters) {
    // Try to read the sensor with multiple attempts
    int attempts = 3;
    DHT11_Data data;
    bool success = false;
    
    while (attempts-- && !success) {
        data = DHT11_Read();
        
        // Check if read was successful
        if (data.humidity != -1 && data.temperature != -1) {
            success = true;
            break;
        }
        
        // If failed but we have more attempts, wait briefly then retry
        if (attempts > 0) {
            ESP_LOGW(TAG, "DHT11 read attempt failed, retrying... (%d attempts left)", attempts);
            vTaskDelay(pdMS_TO_TICKS(500)); // Wait before retry
        }
    }
    
    if (success) {
        // Send to queue only if valid
        xQueueSend(sensor_data_queue, &data, pdMS_TO_TICKS(100));
    } else {
        ESP_LOGE(TAG, "DHT11 read failed after multiple attempts, will retry at next timer interval");
    }

    // Task is self-deleting
    vTaskDelete(NULL);
}

// Task to process sensor data and control the fan and heater
void process_sensor_data_task(void *pvParameters) {
    DHT11_Data sensorData;

    while (1) {
        // Wait for data from the queue (blocking until data arrives)
        if (xQueueReceive(sensor_data_queue, &sensorData, portMAX_DELAY) == pdTRUE) {
            int temperature = sensorData.temperature;
            int humidity = sensorData.humidity;
            
            // Store latest data for web server
            latest_sensor_data = sensorData;
            
            // Control fan and heater based on temperature
            control_fan(temperature);
            control_heater(temperature);
            
            // Display status in serial monitor with clear formatting
            ESP_LOGI(TAG, "Temperature: %d°C, Humidity: %d%%, Fan: %s, Heater: %s, Thresholds: Low: %d°C, High: %d°C",
                   temperature, humidity, 
                   fan_state ? "ON" : "OFF", 
                   heater_state ? "ON" : "OFF",
                   temp_low, temp_high);
        }
    }
}