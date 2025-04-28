/*
 * Integrated ESP32 Climate Control, RFID, and NRF24L01 System
 * Main application with optimized SPI configuration for ESP32-S3
 */

 #include "integrated_system.h"
 #include "servo.h"
 #include "spi_config.h"
 
 static const char *TAG = "integrated-system";
 
 /* Global variables for RFID module */
 QueueHandle_t event_queue;
 spi_device_handle_t rfid_spi; // RFID-specific SPI handle
 volatile bool in_card_processing = false;
 bool last_card_authorized = false;
 char last_card_uid[50] = "None";
 static esp_timer_handle_t correct_rfid_led_timer;
 static esp_timer_handle_t debounce_timer;
 esp_timer_handle_t door_close_timer;  // Timer to close the door after delay
 
 /* Global variables for NRF24L01 module */
 QueueHandle_t nrf_message_queue;
 char last_nrf_message[33] = "None";
 nrf_mode_t nrf_current_mode = NRF_DEFAULT_MODE;
 bool nrf_status_updated = false;
 
 /* RFID LED Timer callback */
 static void led_off_callback(void *arg)
 {
     gpio_set_level(CORRECT_RFID_LED, 0);
     ESP_LOGI(TAG, "RFID LED turned OFF after 2 seconds");
 }
 
 /* Start LED timer */
 static void start_led_timer(void)
 {
     esp_err_t err = esp_timer_stop(correct_rfid_led_timer);
     if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
         ESP_LOGE(TAG, "Failed to stop timer: %d", err);
     }
 
     err = esp_timer_start_once(correct_rfid_led_timer, 2 * 1000 * 1000);
     if (err != ESP_OK) {
         ESP_LOGE(TAG, "Failed to start timer: %d", err);
     }
 }
 
 /* Debounce timer callback */
 static void debounce_timer_callback(void *arg)
 {
     // Re-enable GPIO interrupts after debounce period
     gpio_intr_enable(BUTTON_PIN_ADD);
     gpio_intr_enable(BUTTON_PIN_DELETE);
     gpio_intr_enable(RFID_IRQ_PIN);
 }
 
 /* GPIO interrupt handler */
 static void IRAM_ATTR gpio_isr_handler(void *arg)
 {
     uint32_t gpio_num = (uint32_t)arg;
     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
     event_t event;
     
     // Determine which button was pressed
     if (gpio_num == BUTTON_PIN_ADD) {
         event.type = EVENT_ADD_BUTTON;
     } else if (gpio_num == BUTTON_PIN_DELETE) {
         event.type = EVENT_DELETE_BUTTON;
     } else if (gpio_num == RFID_IRQ_PIN) {
         event.type = EVENT_CARD_DETECTED;
     } else {
         return; // Unknown interrupt source
     }
     
     // Send event to queue
     xQueueSendFromISR(event_queue, &event, &xHigherPriorityTaskWoken);
     
     // Temporarily disable GPIO interrupts to prevent bouncing
     gpio_intr_disable(BUTTON_PIN_ADD);
     gpio_intr_disable(BUTTON_PIN_DELETE);
     gpio_intr_disable(RFID_IRQ_PIN);
     
     // Start debounce timer
     esp_timer_start_once(debounce_timer, DEBOUNCE_TIME * 1000);
     
     // Yield to higher priority task if necessary
     if (xHigherPriorityTaskWoken) {
         portYIELD_FROM_ISR();
     }
 }
 
 /* Door close timer callback */
 static void door_close_callback(void *arg)
 {
     ESP_LOGI(TAG, "Closing door after timeout");
     servo_move(DOOR_CLOSED_ANGLE, DOOR_SERVO_CHANNEL, DOOR_SERVO_MODE);
 }
 
 /* Setup GPIO interrupts for RFID module */
 void setup_gpio_interrupts(void)
 {
     gpio_config_t io_conf = {};
     
     // Setup LED pin
     io_conf.pin_bit_mask = (1ULL << CORRECT_RFID_LED);
     io_conf.mode = GPIO_MODE_OUTPUT;
     io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
     io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
     io_conf.intr_type = GPIO_INTR_DISABLE;
     gpio_config(&io_conf);
     
     // Setup button pins
     io_conf.pin_bit_mask = (1ULL << BUTTON_PIN_ADD) | (1ULL << BUTTON_PIN_DELETE);
     io_conf.mode = GPIO_MODE_INPUT;
     io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
     io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
     io_conf.intr_type = GPIO_INTR_NEGEDGE;  // Interrupt on falling edge (button press)
     gpio_config(&io_conf);
     
     // Setup RFID IRQ pin
     io_conf.pin_bit_mask = (1ULL << RFID_IRQ_PIN);
     io_conf.mode = GPIO_MODE_INPUT;
     io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
     io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
     io_conf.intr_type = GPIO_INTR_NEGEDGE;  // Interrupt when card detected
     gpio_config(&io_conf);
     
     // Install GPIO ISR service
     gpio_install_isr_service(0);
     
     // Hook ISR handler for buttons and RFID IRQ
     gpio_isr_handler_add(BUTTON_PIN_ADD, gpio_isr_handler, (void*) BUTTON_PIN_ADD);
     gpio_isr_handler_add(BUTTON_PIN_DELETE, gpio_isr_handler, (void*) BUTTON_PIN_DELETE);
     gpio_isr_handler_add(RFID_IRQ_PIN, gpio_isr_handler, (void*) RFID_IRQ_PIN);
 }
 
 /* Control the door based on card authorization */
 static void control_door(bool authorized)
 {
     if (authorized) {
         ESP_LOGI(TAG, "Opening door for authorized card");
         servo_move(DOOR_OPEN_ANGLE, DOOR_SERVO_CHANNEL, DOOR_SERVO_MODE);
         
         // Start the timer to close the door after the configured time
         esp_err_t err = esp_timer_stop(door_close_timer);
         if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
             ESP_LOGE(TAG, "Failed to stop door timer: %d", err);
         }
         
         err = esp_timer_start_once(door_close_timer, DOOR_OPEN_TIME_MS * 1000);
         if (err != ESP_OK) {
             ESP_LOGE(TAG, "Failed to start door timer: %d", err);
         }
     } else {
         ESP_LOGI(TAG, "Unauthorized card - door remains closed");
         // Ensure door is closed (may not be necessary but added for safety)
         servo_move(DOOR_CLOSED_ANGLE, DOOR_SERVO_CHANNEL, DOOR_SERVO_MODE);
     }
 }
 
 /* Initialize RFID RC522 module */
 void initialize_rc522(void)
 {
     ESP_LOGI(TAG, "Initializing RC522 RFID reader on RFID_SPI_HOST");
     
     // Configure SPI for RC522
     spi_bus_config_t rfid_bus_cfg = {
         .miso_io_num = RFID_PIN_MISO,
         .mosi_io_num = RFID_PIN_MOSI,
         .sclk_io_num = RFID_PIN_CLK,
         .quadwp_io_num = -1,
         .quadhd_io_num = -1,
         .max_transfer_sz = 0  // default to 4094
     };
     
     spi_device_interface_config_t rfid_dev_cfg = {
         .clock_speed_hz = 5000000,  // 5 MHz
         .mode = 0,                  // SPI mode 0
         .spics_io_num = RFID_PIN_CS, // CS pin
         .queue_size = 7,
         .pre_cb = NULL,
         .post_cb = NULL
     };
     
     // Initialize SPI bus for RFID
     ESP_ERROR_CHECK(spi_bus_initialize(RFID_SPI_HOST, &rfid_bus_cfg, SPI_DMA_CH_AUTO));
     ESP_ERROR_CHECK(spi_bus_add_device(RFID_SPI_HOST, &rfid_dev_cfg, &rfid_spi));
     
     // Initialize RC522
     PCD_Init(rfid_spi);
     
     // Setup RC522 for IRQ mode
     PCD_WriteRegister(rfid_spi, ComIEnReg, 0xA0);  // Enable IRQ for card detection
     
     // Clear all IRQ flags
     PCD_ClearRegisterBitMask(rfid_spi, ComIrqReg, 0x7F);
     
     ESP_LOGI(TAG, "RC522 RFID reader initialized successfully");
 }
 
 /* Initialize NRF24L01 module */
 void initialize_nrf24l01(nrf_mode_t mode)
 {
     ESP_LOGI(TAG, "Initializing nRF24L01 in %s mode on NRF_SPI_HOST...", 
              mode == NRF_MODE_RX ? "receiver" : "transmitter");
     
     // Default pin configuration
     nrf_pins_t pins = NRF_USE_PIN_CONFIG;
     
     // Default radio configuration
     nrf_config_t config = {
         .channel = 0x67,         // RF channel (must be the same on TX and RX)
         .power_rate = 0x06,      // RF_SETUP: 0dBm output power, 1Mbps data rate
         .address = {0x00, 0x00, 0x00, 0x00, 0x01}, // 5-byte address
         .payload_size = 32       // 32-byte payload
     };
     
     // Initialize nRF24L01
     nrf_init(pins, config, mode);
     nrf_check_configuration();
     
     // Save the current mode
     nrf_current_mode = mode;
     
     ESP_LOGI(TAG, "nRF24L01 initialized successfully");
 }
 
 /* Safe access to PICC check and select */
 esp_err_t safe_picc_check_and_select(void)
 {
     esp_err_t result = STATUS_ERROR;
     
     // No need for mutex with separate SPI controllers
     if (PICC_IsNewCardPresent(rfid_spi)) {
         result = PICC_ReadCardSerial(rfid_spi);
         if (result) {
             printf("*** Card detected! ***\n");
             PICC_DumpToSerial(rfid_spi, &uid);
         }
     }
     
     return result;
 }
 
 /* Card processing task */
 void card_processing_task(void *pvParameters)
 {
     bool result;
     char uid_str[50] = {0};
     
     while (1) {
         if (in_card_processing) {
             // No need for mutex with separate SPI controllers
             if (PICC_IsNewCardPresent(rfid_spi)) {
                 result = PICC_ReadCardSerial(rfid_spi);
                 if (result) {
                     printf("*** Card detected! ***\n");
                     PICC_DumpToSerial(rfid_spi, &uid);
                     
                     // Format UID as string
                     int offset = 0;
                     for (uint8_t i = 0; i < uid.size; i++) {
                         offset += snprintf(uid_str + offset, sizeof(uid_str) - offset, 
                                            "%02X", uid.uidByte[i]);
                         if (i < uid.size - 1) {
                             offset += snprintf(uid_str + offset, sizeof(uid_str) - offset, ":");
                         }
                     }
                     
                     bool authorized = is_uid_in_nvs(uid.uidByte, uid.size);
                     if (authorized) {
                         gpio_set_level(CORRECT_RFID_LED, 1);
                         ESP_LOGI(TAG, "Card recognized. LED ON.");
                         start_led_timer();
                         
                         // Control the door - open for authorized cards
                         control_door(true);
                     } else {
                         ESP_LOGI(TAG, "Card not recognized.");
                         
                         // Control the door - ensure closed for unauthorized cards
                         control_door(false);
                     }
                     
                     // Update status for web interface
                     last_card_authorized = authorized;
                     strcpy(last_card_uid, uid_str);
                     update_rfid_status(authorized, uid_str);
                 }
             }
             vTaskDelay(pdMS_TO_TICKS(100));
         } else {
             // Wait until we're asked to process a card
             vTaskDelay(pdMS_TO_TICKS(100));
         }
     }
 }
 
 /* NRF24L01 processing task - Modified from original nrf24_inte.c */
 void nrf_processing_task(void *pvParameters)
{
    static uint8_t buffer[32];  // Use static to avoid stack allocation
    event_t event;
    
    ESP_LOGI(TAG, "NRF24L01 processing task started");
    
    if (nrf_current_mode == NRF_MODE_RX) {
        // Receiver mode task
        while (1) {
            if (nrf_data_available()) {
                // Zero out buffer before receiving new data
                memset(buffer, 0, sizeof(buffer));
                
                // Read data safely
                nrf_read_data(buffer, sizeof(buffer) - 1); // Leave space for null terminator
                buffer[sizeof(buffer) - 1] = '\0'; // Ensure null termination
                
                // Log only if message is valid
                if (strlen((char *)buffer) > 0) {
                    ESP_LOGI(TAG, "Received data via nRF24L01: %s", buffer);
                    
                    // Copy the message to the global buffer for web interface
                    strncpy(last_nrf_message, (char *)buffer, sizeof(last_nrf_message) - 1);
                    last_nrf_message[sizeof(last_nrf_message) - 1] = '\0';
                    
                    // Set update flag for web interface
                    nrf_status_updated = true;
                    
                    // Create an event for the message - keep stack usage minimal
                    event.type = EVENT_NRF_MESSAGE_RECEIVED;
                    memcpy(event.data.nrf_message, buffer, sizeof(event.data.nrf_message));
                    
                    // Process the message
                    nrf_message_handler(buffer, strlen((char *)buffer));
                    
                    // Send the event to the event queue - outside of critical section
                    if (xQueueSend(event_queue, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
                        ESP_LOGE(TAG, "Failed to send NRF message event to queue");
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(50)); // Check more frequently, but use less CPU
        }
    } else {
        // Transmitter mode task
        static uint8_t data[] = "Hello from ESP32 TX!";  // Use static to avoid stack allocation
        while (1) {
            ESP_LOGI(TAG, "Sending data via nRF24L01: %s", data);
            nrf_send_data(data, strlen((char *)data));
            vTaskDelay(pdMS_TO_TICKS(1000)); // Send every second
        }
    }
}
 
 /* Handler for NRF24L01 messages */
 void nrf_message_handler(uint8_t *message, size_t length)
 {
     // Add your message handling logic here
     // For example, you could check for specific commands:
     if (strncmp((char *)message, "OPEN_DOOR", 9) == 0) {
         ESP_LOGI(TAG, "Received door open command via NRF24L01");
         control_door(true);
     } else if (strncmp((char *)message, "CLOSE_DOOR", 10) == 0) {
         ESP_LOGI(TAG, "Received door close command via NRF24L01");
         control_door(false);
     } else {
         ESP_LOGI(TAG, "Received general message via NRF24L01: %.*s", length, message);
     }
 }
 
 /* Update NRF status for web interface */
 void update_nrf_status(const char* message)
 {
     nrf_status_updated = true;
     ESP_LOGI(TAG, "NRF24L01 status updated for web interface: %s", message);
 }
 
 /* Event handling task */
 void event_handling_task(void *pvParameters)
 {
     event_t event;
     int64_t start;
     bool card_read;
     bool result;
     
     while (1) {
         if (xQueueReceive(event_queue, &event, portMAX_DELAY)) {
             switch (event.type) {             
                 case EVENT_ADD_BUTTON:
                     ESP_LOGI(TAG, "ADD Button Pressed");
                     in_card_processing = false;  // Stop regular scanning
                     
                     start = esp_timer_get_time();
                     card_read = false;
                     
                     while ((esp_timer_get_time() - start) < ADD_CARD_TIMEOUT * 1000 && !card_read) {
                         result = safe_picc_check_and_select();
                         if (result) {
                             card_read = true;
                             
                             if (is_uid_unique(uid.uidByte, uid.size)) {
                                 // No mutex needed with separate SPI controllers
                                 store_uid_in_nvs(uid.uidByte, uid.size);
                                 ESP_LOGI(TAG, "UID added to storage.");
                                 display_all_uids();
                             } else {
                                 ESP_LOGI(TAG, "UID already exists in storage. Not storing.");
                             }
                             break;
                         }
                         vTaskDelay(pdMS_TO_TICKS(50));
                     }
                     
                     if (!card_read) {
                         ESP_LOGI(TAG, "No card within timeout.");
                     }
                     
                     ESP_LOGI(TAG, "Returning to normal scanning...");
                     in_card_processing = true;  // Resume regular scanning
                     break;
                     
                 case EVENT_DELETE_BUTTON:
                     ESP_LOGI(TAG, "DELETE Button Pressed");
                     in_card_processing = false;  // Stop regular scanning
                     
                     start = esp_timer_get_time();
                     card_read = false;
                     
                     while ((esp_timer_get_time() - start) < ADD_CARD_TIMEOUT * 1000 && !card_read) {
                         result = safe_picc_check_and_select();
                         if (result) {
                             card_read = true;
                             
                             // No mutex needed with separate SPI controllers
                             delete_uid_in_nvs(uid.uidByte, uid.size);
                             ESP_LOGI(TAG, "UID deleted (if existed).");
                             display_all_uids();
                             break;
                         }
                         vTaskDelay(pdMS_TO_TICKS(50));
                     }
                     
                     if (!card_read) {
                         ESP_LOGI(TAG, "No card within timeout.");
                     }
                     
                     ESP_LOGI(TAG, "Returning to normal scanning...");
                     in_card_processing = true;  // Resume regular scanning
                     break;
                     
                 case EVENT_CARD_DETECTED:
                     // This event is generated by the RFID IRQ pin
                     // The card_processing_task will handle the actual card reading
                     break;
                     
                 case EVENT_NRF_MESSAGE_RECEIVED:
                     // Process the NRF24L01 message (already handled in nrf_processing_task)
                     break;
                     
                 default:
                     ESP_LOGI(TAG, "Unknown event type");
                     break;
             }
         }
     }
 }
 
 /* Main application */
 void app_main(void)
 {
     // Initialize NVS - required by all subsystems
     esp_err_t ret = nvs_flash_init();
     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         ESP_ERROR_CHECK(nvs_flash_erase());
         ret = nvs_flash_init();
     }
     ESP_ERROR_CHECK(ret);
     
     ESP_LOGI(TAG, "Initializing integrated climate control, RFID, and NRF24L01 system");
     
     // ===== Initialize Climate Control System =====
     
     // Initialize hardware
     init_control_pins();
     DHT11_Init(DHT_GPIO);
 
     // Create queue for sensor data
     sensor_data_queue = xQueueCreate(5, sizeof(DHT11_Data));
 
     // Create timer for sensor readings (2 seconds interval)
     sensor_reading_timer = xTimerCreate(
         "SensorReadingTimer",           // Timer name
         pdMS_TO_TICKS(2000),            // Timer period (2 seconds)
         pdTRUE,                         // Auto-reload
         NULL,                           // Timer ID
         sensor_reading_timer_callback   // Callback function
     );
 
     // ===== Initialize RFID System =====
     
     // Create event queue
     event_queue = xQueueCreate(10, sizeof(event_t));
     
     if (event_queue == NULL) {
         ESP_LOGE(TAG, "Failed to create event queue");
         return;
     }
     
     // Create timers for RFID system
     esp_timer_create_args_t led_timer_args = {
         .callback = led_off_callback,
         .name = "led_off_timer"
     };
     ESP_ERROR_CHECK(esp_timer_create(&led_timer_args, &correct_rfid_led_timer));
     
     esp_timer_create_args_t debounce_timer_args = {
         .callback = debounce_timer_callback,
         .name = "debounce_timer"
     };
     ESP_ERROR_CHECK(esp_timer_create(&debounce_timer_args, &debounce_timer));
     
     // Initialize RC522 RFID reader
     initialize_rc522();
     
     // Set up GPIOs and interrupts for RFID
     setup_gpio_interrupts();
     
     // Show stored UIDs at startup
     ESP_LOGI(TAG, "Displaying stored UIDs at startup:");
     display_all_uids();
     
     // Start normal card processing mode
     in_card_processing = true;
     
     // ===== Initialize NRF24L01 System =====
     
     // Create message queue for NRF24L01
     nrf_message_queue = xQueueCreate(NRF_MESSAGE_QUEUE_SIZE, 32); // 32 bytes per message
     
     if (nrf_message_queue == NULL) {
         ESP_LOGE(TAG, "Failed to create NRF message queue");
         return;
     }
     
     // Initialize NRF24L01 module
     initialize_nrf24l01(NRF_DEFAULT_MODE);
     
     // ===== Create FreeRTOS Tasks =====
     
     // Start the climate control timer
     xTimerStart(sensor_reading_timer, 0);
 
     // Create task to process sensor data
     xTaskCreate(process_sensor_data_task, "process_sensor_data", 4096, NULL, 4, NULL);
 
     // Optionally trigger an immediate first reading
     xTaskCreate(read_dht11_task, "initial_dht11_read", 4096, NULL, 5, NULL);
     
     // Create RFID tasks
     xTaskCreate(card_processing_task, "card_task", 4096, NULL, 5, NULL);
     xTaskCreate(event_handling_task, "event_task", 4096, NULL, 4, NULL);
     
     // Create NRF24L01 task
     xTaskCreate(nrf_processing_task, "nrf_task", NRF_TASK_STACK_SIZE, NULL, NRF_TASK_PRIORITY, NULL);
     
     // Initialize WiFi and web server (enhanced version with RFID support)
     wifi_webserver_init();
 
     // Initialize the servo for door control
     servo_init(DOOR_SERVO_PIN, DOOR_SERVO_CHANNEL, DOOR_SERVO_TIMER, DOOR_SERVO_MODE);
 
     // Ensure door is closed at startup
     servo_move(DOOR_CLOSED_ANGLE, DOOR_SERVO_CHANNEL, DOOR_SERVO_MODE);
 
     // Create timer for door closing
     esp_timer_create_args_t door_timer_args = {
         .callback = door_close_callback,
         .name = "door_close_timer"
     };
     ESP_ERROR_CHECK(esp_timer_create(&door_timer_args, &door_close_timer));
 
     ESP_LOGI(TAG, "Door control initialized");
     
     ESP_LOGI(TAG, "Integrated system running with NRF24L01 in %s mode", 
              nrf_current_mode == NRF_MODE_RX ? "receiver" : "transmitter");
     ESP_LOGI(TAG, "Connect to WiFi network: %s", WIFI_AP_SSID);
 }