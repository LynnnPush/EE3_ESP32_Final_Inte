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

#ifndef NRF24_INTE_H
#define NRF24_INTE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Operation mode
typedef enum {
    NRF_MODE_RX = 0,  // Receiver mode
    NRF_MODE_TX = 1   // Transmitter mode
} nrf_mode_t;

// Pin configuration structure
typedef struct {
    int ce_pin;
    int csn_pin;
    int sck_pin;
    int miso_pin;
    int mosi_pin;
} nrf_pins_t;

// nRF24L01 configuration parameters
typedef struct {
    uint8_t channel;         // RF channel (0-125)
    uint8_t power_rate;      // RF_SETUP value (power and data rate)
    uint8_t address[5];      // Default address for TX and RX pipe 0
    uint8_t payload_size;    // Size of the payload (1-32 bytes)
} nrf_config_t;

// Pin configurations
#define PIN_CONFIG_BUILD1 {  \
    .ce_pin = 4,             \
    .csn_pin = 5,            \
    .sck_pin = 7,            \
    .miso_pin = 16,          \
    .mosi_pin = 15           \
}

#define PIN_CONFIG_FLYNN {   \
    .ce_pin = 9,             \
    .csn_pin = 10,           \
    .sck_pin = 12,           \
    .miso_pin = 13,          \
    .mosi_pin = 11           \
}

// Function prototypes
void nrf_init(nrf_pins_t pins, nrf_config_t config, nrf_mode_t mode);
void nrf_send_data(uint8_t *data, size_t length);
bool nrf_data_available(void);
void nrf_read_data(uint8_t *data, size_t length);
void nrf_check_configuration(void);

// Task functions - export these for main.c to use
void tx_task(void *pvParameters);
void rx_task(void *pvParameters);

#endif // NRF24_INTE_H