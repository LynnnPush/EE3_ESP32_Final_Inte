/**
 * @title NRF24L01 Transceiver Application
 * @file main.c
 * @author Shanghong Lin (BUILD1)
 * @original Feiyang Zheng (GAME2)
 * @AI Use Claude3.7 Sonnet for reformatting the code
 * @version 0.1
 * @date 2025-04-24
 * 
 */

#include <stdio.h>
#include <string.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nrf24_inte.h"
#include "integrated_system.h"
#include "spi_config.h"

// nRF24L01 register addresses
#define NRF_REG_CONFIG      0x00
#define NRF_REG_EN_AA       0x01
#define NRF_REG_RF_SETUP    0x06
#define NRF_REG_RF_CH       0x05
#define NRF_REG_STATUS      0x07
#define NRF_REG_RX_ADDR_P0  0x0A
#define NRF_REG_TX_ADDR     0x10
#define NRF_REG_RX_PW_P0    0x11
#define NRF_REG_FIFO_STATUS 0x17

// nRF24L01 commands
#define NRF_CMD_R_REGISTER   0x00
#define NRF_CMD_W_REGISTER   0x20
#define NRF_CMD_R_RX_PAYLOAD 0x61
#define NRF_CMD_W_TX_PAYLOAD 0xA0
#define NRF_CMD_FLUSH_TX     0xE1
#define NRF_CMD_FLUSH_RX     0xE2

// SPI configuration
#define SPI_HOST SPI2_HOST
#define SPI_CLOCK_SPEED 1000000  // 1 MHz

// Global variables
spi_device_handle_t nrf_spi;
static nrf_pins_t nrf_pins;
static nrf_mode_t current_mode;

// Initialize GPIO pins
static void nrf_gpio_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << nrf_pins.ce_pin) | (1ULL << nrf_pins.csn_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(nrf_pins.ce_pin, 0);   // Default CE low
    gpio_set_level(nrf_pins.csn_pin, 1);  // Default CSN high
}

// Initialize SPI
static void spi_init(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = nrf_pins.miso_pin,
        .mosi_io_num = nrf_pins.mosi_pin,
        .sclk_io_num = nrf_pins.sck_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    
    spi_device_interface_config_t devcfg = { 
        .clock_speed_hz = SPI_CLOCK_SPEED,
        .mode = 0, // SPI mode 0
        .spics_io_num = -1, // Manually control CSN
        .queue_size = 7,
    };

    // Initialize SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(NRF_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(NRF_SPI_HOST, &devcfg, &nrf_spi));
}

// Write to nRF24L01 register
static void nrf_write_register(uint8_t reg, uint8_t value) {
    uint8_t tx_data[2] = { NRF_CMD_W_REGISTER | reg, value };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
    };
    gpio_set_level(nrf_pins.csn_pin, 0); // CSN low to start communication
    spi_device_transmit(nrf_spi, &t);
    gpio_set_level(nrf_pins.csn_pin, 1); // CSN high to end communication
}

// Write multiple bytes to nRF24L01 register
static void nrf_write_register_multi(uint8_t reg, uint8_t *data, size_t length) {
    uint8_t tx_data[length + 1];
    tx_data[0] = NRF_CMD_W_REGISTER | reg;
    memcpy(&tx_data[1], data, length);
    spi_transaction_t t = {
        .length = (length + 1) * 8,
        .tx_buffer = tx_data,
    };
    gpio_set_level(nrf_pins.csn_pin, 0); // CSN low to start communication
    spi_device_transmit(nrf_spi, &t);
    gpio_set_level(nrf_pins.csn_pin, 1); // CSN high to end communication
}

// Read from nRF24L01 register
static uint8_t nrf_read_register(uint8_t reg) {
    uint8_t tx_data[2] = { NRF_CMD_R_REGISTER | reg, 0xFF };
    uint8_t rx_data[2] = {0};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx_data,
        .rx_buffer = rx_data,
    };
    gpio_set_level(nrf_pins.csn_pin, 0); // CSN low to start communication
    spi_device_transmit(nrf_spi, &t);
    gpio_set_level(nrf_pins.csn_pin, 1); // CSN high to end communication
    return rx_data[1];
}

// Read multiple bytes from nRF24L01 register
static void nrf_read_register_multi(uint8_t reg, uint8_t *data, size_t length) {
    uint8_t tx_data = NRF_CMD_R_REGISTER | reg;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &tx_data,
    };
    gpio_set_level(nrf_pins.csn_pin, 0); // CSN low to start communication
    spi_device_transmit(nrf_spi, &t);  // Send register address

    t.length = length * 8;
    t.tx_buffer = NULL;
    t.rx_buffer = data;
    spi_device_transmit(nrf_spi, &t);  // Receive data
    gpio_set_level(nrf_pins.csn_pin, 1); // CSN high to end communication
}

// Write payload to nRF24L01 (for TX mode)
static void nrf_write_payload(uint8_t *data, size_t length) {
    uint8_t cmd = NRF_CMD_W_TX_PAYLOAD;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    gpio_set_level(nrf_pins.csn_pin, 0); // CSN low to start communication
    spi_device_transmit(nrf_spi, &t);  // Send write payload command

    t.length = length * 8;
    t.tx_buffer = data;
    spi_device_transmit(nrf_spi, &t);  // Send data
    gpio_set_level(nrf_pins.csn_pin, 1); // CSN high to end communication
}

// Read payload from nRF24L01 (for RX mode)
static void nrf_read_payload(uint8_t *data, size_t length) {
    uint8_t cmd = NRF_CMD_R_RX_PAYLOAD;
    spi_transaction_t t = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    gpio_set_level(nrf_pins.csn_pin, 0); // CSN low to start communication
    spi_device_transmit(nrf_spi, &t);  // Send read payload command
    memset (&t, 0, sizeof(t));

    t.length = length * 8;
    t.tx_buffer = NULL;
    t.rx_buffer = data;
    spi_device_transmit(nrf_spi, &t);  // Receive data
    gpio_set_level(nrf_pins.csn_pin, 1); // CSN high to end communication
}

// Clear all interrupt flags
static void nrf_clear_interrupts(void) {
    nrf_write_register(NRF_REG_STATUS, 0x70); // Clear RX_DR, TX_DS, MAX_RT flags
}

// Initialize nRF24L01
void nrf_init(nrf_pins_t pins, nrf_config_t config, nrf_mode_t mode) {
    // Save configuration values
    nrf_pins = pins;
    current_mode = mode;
    
    // Initialize GPIO and SPI
    nrf_gpio_init();
    spi_init();
    
    // Set common configuration
    nrf_write_register(NRF_REG_RF_SETUP, config.power_rate);
    nrf_write_register(NRF_REG_RF_CH, config.channel);
    nrf_write_register_multi(NRF_REG_TX_ADDR, config.address, 5);
    nrf_write_register_multi(NRF_REG_RX_ADDR_P0, config.address, 5);
    nrf_write_register(NRF_REG_RX_PW_P0, config.payload_size);
    nrf_write_register(NRF_REG_EN_AA, 0x01);  // Enable auto-ack on pipe 0
    
    // Mode-specific configuration
    if (mode == NRF_MODE_RX) {
        // RX mode: PWR_UP=1, PRIM_RX=1
        nrf_write_register(NRF_REG_CONFIG, 0x0F);
        // Set CE high to start receiving
        gpio_set_level(nrf_pins.ce_pin, 1);
    } else {
        // TX mode: PWR_UP=1, PRIM_RX=0
        nrf_write_register(NRF_REG_CONFIG, 0x0E);
        // Keep CE low until we send data
        gpio_set_level(nrf_pins.ce_pin, 0);
    }
}

// Send data (TX mode)
void nrf_send_data(uint8_t *data, size_t length) {
    if (current_mode != NRF_MODE_TX) {
        printf("Error: Not in TX mode\n");
        return;
    }
    
    // Prepare data buffer (pad with zeros if needed)
    uint8_t tx_buffer[32] = {0};
    if (length > 32) length = 32;
    memcpy(tx_buffer, data, length);
    
    // Write payload and start transmission
    nrf_write_payload(tx_buffer, 32);
    gpio_set_level(nrf_pins.ce_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(1));  // Minimum 10µs pulse, using 1ms for safety
    gpio_set_level(nrf_pins.ce_pin, 0);
    
    // Wait for transmission to complete
    uint8_t status = 0;
    int timeout = 10;  // 10ms timeout
    
    while (timeout-- > 0) {
        status = nrf_read_register(NRF_REG_STATUS);
        if (status & (1 << 5) || status & (1 << 4)) {
            break;  // TX_DS or MAX_RT set
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Check status
    if (status & (1 << 5)) {
        printf("Data sent successfully\n");
    } else if (status & (1 << 4)) {
        printf("Max retransmission reached\n");
        // Flush TX FIFO
        uint8_t cmd = NRF_CMD_FLUSH_TX;
        spi_transaction_t t = {
            .length = 8,
            .tx_buffer = &cmd,
        };
        gpio_set_level(nrf_pins.csn_pin, 0);
        spi_device_transmit(nrf_spi, &t);
        gpio_set_level(nrf_pins.csn_pin, 1);
    } else {
        printf("Transmission failed\n");
    }
    
    // Clear interrupts
    nrf_clear_interrupts();
}

// Check if data is available (RX mode)
bool nrf_data_available(void) {
    if (current_mode != NRF_MODE_RX) {
        printf("Error: Not in RX mode\n");
        return false;
    }
    
    uint8_t status = nrf_read_register(NRF_REG_STATUS);
    return (status & (1 << 6)) != 0;  // Check RX_DR bit
}

// Read received data (RX mode)
void nrf_read_data(uint8_t *data, size_t length) {
    if (current_mode != NRF_MODE_RX) {
        printf("Error: Not in RX mode\n");
        return;
    }
    
    nrf_read_payload(data, length);
    nrf_clear_interrupts();
}

// Check and print nRF24L01 configuration
void nrf_check_configuration(void) {
    printf("=== Checking nRF24L01 Configuration ===\n");
    
    // Read CONFIG register
    uint8_t config = nrf_read_register(NRF_REG_CONFIG);
    printf("CONFIG: 0x%02X\n", config);
    if (current_mode == NRF_MODE_RX) {
        printf("Mode: Receiver (PRIM_RX=1)\n");
    } else {
        printf("Mode: Transmitter (PRIM_RX=0)\n");
    }
    
    // Read EN_AA register
    uint8_t en_aa = nrf_read_register(NRF_REG_EN_AA);
    printf("EN_AA: 0x%02X (Auto-Ack)\n", en_aa);
    
    // Read RF_SETUP register
    uint8_t rf_setup = nrf_read_register(NRF_REG_RF_SETUP);
    printf("RF_SETUP: 0x%02X (RF Settings)\n", rf_setup);
    
    // Read RF_CH register
    uint8_t rf_ch = nrf_read_register(NRF_REG_RF_CH);
    printf("RF_CH: 0x%02X (RF Channel)\n", rf_ch);
    
    // Read STATUS register
    uint8_t status = nrf_read_register(NRF_REG_STATUS);
    printf("STATUS: 0x%02X\n", status);
    
    // Read RX_ADDR_P0 register
    uint8_t rx_addr[5];
    nrf_read_register_multi(NRF_REG_RX_ADDR_P0, rx_addr, 5);
    printf("RX_ADDR_P0: %02X %02X %02X %02X %02X\n", 
           rx_addr[0], rx_addr[1], rx_addr[2], rx_addr[3], rx_addr[4]);
    
    // Read TX_ADDR register
    uint8_t tx_addr[5];
    nrf_read_register_multi(NRF_REG_TX_ADDR, tx_addr, 5);
    printf("TX_ADDR: %02X %02X %02X %02X %02X\n", 
           tx_addr[0], tx_addr[1], tx_addr[2], tx_addr[3], tx_addr[4]);
    
    printf("=== Configuration Check Complete ===\n");
}

// TX task function
void tx_task(void *pvParameters) {
    uint8_t data[] = "Hello from ESP32 TX!";
    while(1) {
        printf("Sending data: %s\n", data);
        nrf_send_data(data, strlen((char *)data));
        vTaskDelay(pdMS_TO_TICKS(1000)); // Send every second
    }
}

// RX task function
void rx_task(void *pvParameters) {
    uint8_t rx_buffer[32];
    while(1) {
        if (nrf_data_available()) {
            nrf_read_data(rx_buffer,32);
            rx_buffer[31] = '\0'; // Ensure null termination
            printf("Received data: %s\n", rx_buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // Check every 100ms
    }
}