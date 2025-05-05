#include "DHT11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#define TAG "DHT11"
#define DHT11_TIMEOUT_US 100    // Microsecond timeout for signal transitions

static gpio_num_t dht_gpio;

void DHT11_Init(gpio_num_t gpio) {
    dht_gpio = gpio;
    
    // Configure GPIO with internal pull-up
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT_OD,  // Open drain mode for bidirectional use
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Set initial high level 
    gpio_set_level(dht_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); // Give sensor some time to stabilize at startup
}

static int DHT11_Check_Response() {
    int timeout = 0;
    
    // Wait for sensor to pull low (80µs)
    while (gpio_get_level(dht_gpio) == 1) {
        if (++timeout > DHT11_TIMEOUT_US) {
            ESP_LOGE(TAG, "No response from DHT11 (initial low pulse)");
            return 0;
        }
        esp_rom_delay_us(1);
    }
    
    timeout = 0;
    // Wait for sensor to pull high (80µs)
    while (gpio_get_level(dht_gpio) == 0) {
        if (++timeout > DHT11_TIMEOUT_US) {
            ESP_LOGE(TAG, "No response from DHT11 (low to high transition)");
            return 0;
        }
        esp_rom_delay_us(1);
    }
    
    timeout = 0;
    // Wait for sensor to pull low again to start data transmission
    while (gpio_get_level(dht_gpio) == 1) {
        if (++timeout > DHT11_TIMEOUT_US) {
            ESP_LOGE(TAG, "No response from DHT11 (high to low before data)");
            return 0;
        }
        esp_rom_delay_us(1);
    }
    
    return 1;
}

static uint8_t DHT11_Read_Byte() {
    uint8_t data = 0;
    
    for (int i = 0; i < 8; i++) {
        int timeout = 0;
        
        // Wait for rising edge (start of bit)
        while (gpio_get_level(dht_gpio) == 0) {
            if (++timeout > DHT11_TIMEOUT_US) {
                ESP_LOGE(TAG, "Timeout waiting for bit start (rising edge)");
                return 0;
            }
            esp_rom_delay_us(1);
        }
        
        // Wait ~30µs and check level to determine bit value
        esp_rom_delay_us(30);
        
        // If high, it's a 1 bit (70µs high pulse), otherwise it's 0 (26-28µs high pulse)
        if (gpio_get_level(dht_gpio) == 1) {
            data |= (1 << (7 - i));
        }
        
        timeout = 0;
        // Wait for falling edge (end of bit)
        while (gpio_get_level(dht_gpio) == 1) {
            if (++timeout > DHT11_TIMEOUT_US) {
                ESP_LOGE(TAG, "Timeout waiting for bit end (falling edge)");
                return 0;
            }
            esp_rom_delay_us(1);
        }
    }
    
    return data;
}

DHT11_Data DHT11_Read() {
    DHT11_Data result = { .temperature = -1, .humidity = -1 };
    
    // Pull low for at least 18ms to signal start of communication
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(20)); // 20ms low signal
    
    // Release line and wait for sensor response
    gpio_set_level(dht_gpio, 1);
    esp_rom_delay_us(40); // Wait before switching to input mode
    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT);
    
    // Check if sensor responded
    if (!DHT11_Check_Response()) {
        ESP_LOGE(TAG, "DHT11 sensor not responding");
        return result;
    }
    
    // Read 5 bytes: humidity high/low, temperature high/low, checksum
    uint8_t data[5];
    for (int i = 0; i < 5; i++) {
        data[i] = DHT11_Read_Byte();
    }
    
    // Verify checksum
    if (data[4] == (data[0] + data[1] + data[2] + data[3])) {
        result.humidity = data[0];      // DHT11 only uses the integer part
        result.temperature = data[2];   // DHT11 only uses the integer part
        ESP_LOGI(TAG, "DHT11 read successful: Temp=%d°C, Humidity=%d%%", 
                 result.temperature, result.humidity);
    } else {
        ESP_LOGE(TAG, "DHT11 checksum error: %d != %d + %d + %d + %d", 
                data[4], data[0], data[1], data[2], data[3]);
    }
    
    return result;
}
