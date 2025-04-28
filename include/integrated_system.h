/*
 * Integrated ESP32 Climate Control, RFID, and NRF24L01 System
 * Header file containing all necessary definitions
 */

 #ifndef INTEGRATED_SYSTEM_H
 #define INTEGRATED_SYSTEM_H
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "freertos/semphr.h"
 #include "freertos/timers.h"
 #include "driver/gpio.h"
 #include "esp_log.h"
 #include "esp_system.h"
 #include "esp_event.h"
 #include "nvs_flash.h"
 #include "nvs.h"
 #include "esp_timer.h"
 #include "esp_intr_alloc.h"
 #include "driver/spi_master.h"
 #include "esp_http_server.h"
 #include "esp_wifi.h"
 #include "lwip/err.h"
 #include "lwip/sys.h"
 #include "esp_netif.h"
 #include "driver/ledc.h"
 
 /* Include existing module headers */
 #include "DHT11.h"
 #include "climate_control.h"
 #include "MFRC522.h"
 #include "UID.h"
 #include "nrf24_inte.h" /* Added NRF24L01 header */
 #include "spi_config.h"

 /*
  * GPIO Pin Definitions
  * Ensure no conflicts between climate control, RFID, and NRF24L01 components
  */
 /* Climate Control Pins (already defined in climate_control.h) */
 // FAN_GPIO
 // HEATER_GPIO
 // DHT_GPIO
 
 /* RFID GPIO Pins */
 #define BUTTON_PIN_ADD      9
 #define BUTTON_PIN_DELETE   13
 #define CORRECT_RFID_LED    16
 #define RFID_IRQ_PIN        2   // Connect RC522 IRQ pin to this GPIO
 
 /* RFID Settings */
 #define MAX_UIDS            100
 #define UID_LENGTH          16
 #define ADD_CARD_TIMEOUT    2000    // Timeout in milliseconds
 #define DEBOUNCE_TIME       50      // Debounce time in milliseconds
 
 /*Servo door control settings*/ 
 #define DOOR_SERVO_PIN 47            // GPIO pin for the door servo
 #define DOOR_SERVO_CHANNEL LEDC_CHANNEL_0  // LEDC channel for the servo
 #define DOOR_SERVO_TIMER LEDC_TIMER_0      // LEDC timer for the servo
 #define DOOR_SERVO_MODE LEDC_LOW_SPEED_MODE  // LEDC mode
 #define DOOR_OPEN_ANGLE 90           // Angle when door is open (adjust as needed)
 #define DOOR_CLOSED_ANGLE 0          // Angle when door is closed (adjust as needed)
 #define DOOR_OPEN_TIME_MS 5000       // Time to keep door open in milliseconds
 
 /* NRF24L01 Settings */
 #define NRF_USE_PIN_CONFIG PIN_CONFIG_BUILD1  // Use BUILD1 pin config by default
 #define NRF_MESSAGE_QUEUE_SIZE 10    // Size of the NRF24 message queue
 #define NRF_DEFAULT_MODE NRF_MODE_RX // Default mode is receiver
 #define NRF_TASK_STACK_SIZE 8192     // Stack size for NRF24 tasks
 #define NRF_TASK_PRIORITY 5          // Priority of NRF24 tasks
 
 /* WiFi AP Settings */
 #define WIFI_AP_SSID        "ESP32-BUILD1"
 #define WIFI_AP_PASS        "password123"
 
 /* Event types for the event queue */
 typedef enum {
     EVENT_ADD_BUTTON,
     EVENT_DELETE_BUTTON,
     EVENT_CARD_DETECTED,
     EVENT_NRF_MESSAGE_RECEIVED  // Added new event type for NRF24L01
 } event_type_t;
 
 typedef struct {
     event_type_t type;
     union {
         uint8_t nrf_message[32];  // Buffer for NRF24L01 message
     } data;
 } event_t;
 
 /*
  * External variables declarations
  */
 /* Climate Control */
 extern QueueHandle_t sensor_data_queue;
 extern TimerHandle_t sensor_reading_timer;
 extern bool fan_state;
 extern bool heater_state;
 extern bool fan_override;
 extern bool heater_override;
 extern DHT11_Data latest_sensor_data;
 
 /* RFID */
 extern QueueHandle_t event_queue;
 extern SemaphoreHandle_t spi_mutex;
 extern spi_device_handle_t rfid_spi;  // For RFID operations
 extern spi_device_handle_t nrf_spi;   // For NRF24 operations
 extern Uid uid;
 extern volatile bool in_card_processing;
 extern bool last_card_authorized;
 extern char last_card_uid[50];
 
 /* NRF24L01 */
 extern QueueHandle_t nrf_message_queue;
 extern char last_nrf_message[33]; // 32 bytes message + null terminator
 extern nrf_mode_t nrf_current_mode;
 extern bool nrf_status_updated;
 
 /*
  * Function declarations
  */
 /* Climate Control */
 void init_control_pins(void);
 void set_fan_state(bool state);
 void set_heater_state(bool state);
 void control_fan(int temperature);
 void control_heater(int temperature);
 void sensor_reading_timer_callback(TimerHandle_t xTimer);
 void read_dht11_task(void *pvParameters);
 void process_sensor_data_task(void *pvParameters);
 void DHT11_Init(gpio_num_t gpio);
 DHT11_Data DHT11_Read(void);
 
 /* RFID Module */
 void initialize_rc522(void);
 void setup_gpio_interrupts(void);
 void card_processing_task(void *pvParameters);
 void event_handling_task(void *pvParameters);
 esp_err_t safe_picc_check_and_select(void);
 bool is_uid_in_nvs(uint8_t *uid, uint8_t uid_length);
 void store_uid_in_nvs(uint8_t *uid, uint8_t uid_length);
 void delete_uid_in_nvs(uint8_t *uid, size_t uid_length);
 void display_all_uids(void);
 
 /* NRF24L01 Module */
 void initialize_nrf24l01(nrf_mode_t mode);
 void nrf_processing_task(void *pvParameters);
 void nrf_message_handler(uint8_t *message, size_t length);
 void update_nrf_status(const char* message);
 
 /* Web Server */
 void wifi_webserver_init(void);
 void update_rfid_status(bool authorized, const char* uid_str);
 
 #endif /* INTEGRATED_SYSTEM_H */