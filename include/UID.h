#ifndef UID_H
#define UID_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"

#define MAX_UIDS 100
#define UID_LENGTH 16

// Function prototypes
int is_uid_unique(uint8_t *new_uid, size_t uid_length);
void delete_uid_in_nvs(uint8_t *uid, size_t uid_length);
void store_uid_in_nvs(uint8_t *uid, uint8_t uid_length);
void display_all_uids(void);
bool is_uid_in_nvs(uint8_t *uid, uint8_t uid_length);

#endif // UID_H
