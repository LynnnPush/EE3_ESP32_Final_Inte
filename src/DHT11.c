#include "DHT11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

#define TAG "DHT11"

#define DHT11_MAX_EDGES      100
#define DHT11_EXPECTED_EDGES 82
#define PULSE_THRESHOLD      50

static gpio_num_t dht_gpio;
static volatile uint32_t edge_times[DHT11_MAX_EDGES];
static volatile int edge_count = 0;
static SemaphoreHandle_t dht11_sem = NULL;

// ISR for handling GPIO edge events
static void IRAM_ATTR dht11_isr_handler(void *arg)
{
    if (edge_count < DHT11_MAX_EDGES) {
        edge_times[edge_count] = esp_timer_get_time();
        edge_count++;
    }

    if (edge_count >= DHT11_EXPECTED_EDGES) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(dht11_sem, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

// Initialize DHT11 sensor & configure GPIO interrupt
void DHT11_Init(gpio_num_t gpio)
{
    dht_gpio = gpio;

    if (!dht11_sem) {
        dht11_sem = xSemaphoreCreateBinary();
    }

    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT);
    gpio_set_intr_type(dht_gpio, GPIO_INTR_ANYEDGE);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(dht_gpio, dht11_isr_handler, NULL);

    gpio_set_level(dht_gpio, 1);
}

// Send start signal to sensor
static void DHT11_Start_Signal()
{
    gpio_isr_handler_remove(dht_gpio);

    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(dht_gpio, 1);
    esp_rom_delay_us(40);

    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT);

    edge_count = 0;

    gpio_isr_handler_add(dht_gpio, dht11_isr_handler, NULL);
}

// Read data from sensor using interrupt-based approach
DHT11_Data DHT11_Read()
{
    DHT11_Data result = { .temperature = -1, .humidity = -1 };

    DHT11_Start_Signal();

    if (xSemaphoreTake(dht11_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGE(TAG, "DHT11 read timeout");
        gpio_isr_handler_remove(dht_gpio);
        return result;
    }

    gpio_isr_handler_remove(dht_gpio);

    if (edge_count < DHT11_EXPECTED_EDGES) {
        ESP_LOGE(TAG, "Incomplete DHT11 signal: %d edges captured", edge_count);
        return result;
    }

    uint8_t data[5] = {0};
    for (int i = 0; i < 40; i++) {
        int idx_rising = 2 + (2 * i);
        int idx_falling = idx_rising + 1;
        uint32_t pulse_duration = edge_times[idx_falling] - edge_times[idx_rising];

        if (pulse_duration > PULSE_THRESHOLD) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    uint8_t checksum = data[0] + data[1] + data[2] + data[3];
    if (checksum != data[4]) {
        ESP_LOGE(TAG, "Checksum error: expected %d, got %d", data[4], checksum);
        ESP_LOGE(TAG, "Decoded bytes: %d %d %d %d", data[0], data[1], data[2], data[3]);
        return result;
    }

    result.humidity    = data[0];
    result.temperature = data[2];
    return result;
}
