// UID.c
#include "UID.h"

static const char *TAG = "RC522";

int is_uid_unique(uint8_t *new_uid, size_t uid_length)
{
    esp_err_t err;
    nvs_handle_t my_handle;

    uint8_t stored_uids[MAX_UIDS][UID_LENGTH];
    size_t stored_uid_size = sizeof(stored_uids);
    size_t uid_count = 0;

    // Open NVS in read mode
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return 1; // If error occurs, assume UID is unique to proceed
    }

    // Read the stored UIDs
    err = nvs_get_blob(my_handle, "card_uids", stored_uids, &stored_uid_size);
    if (err == ESP_OK)
    {
        uid_count = stored_uid_size / sizeof(stored_uids[0]);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        // No UIDs stored yet, so this one is unique
        nvs_close(my_handle);
        return 1;
    }
    else
    {
        ESP_LOGE(TAG, "Error reading stored UIDs: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return 1; // Proceed with storing if there was an error reading
    }

    // Check if the new UID already exists
    for (size_t i = 0; i < uid_count; i++)
    {
        if (memcmp(stored_uids[i], new_uid, uid_length) == 0)
        {
            nvs_close(my_handle);
            return 0; // UID already exists
        }
    }

    // UID is unique
    nvs_close(my_handle);
    return 1;
}

/*
 *----------------- delete_uid_in_nvs -----------------
 * Delete a UID from the NVS storage
 */
void delete_uid_in_nvs(uint8_t *uid, size_t uid_length)
{
    esp_err_t err;
    nvs_handle_t my_handle;

    // Open NVS in read-write mode
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    // Read the existing list of UIDs
    uint8_t stored_uids[MAX_UIDS][UID_LENGTH]; 
    size_t uid_count = 0;
    size_t stored_uid_size = sizeof(stored_uids);

    err = nvs_get_blob(my_handle, "card_uids", stored_uids, &stored_uid_size);
    if (err == ESP_OK)
    {
        // Calculate how many UIDs are stored
        uid_count = stored_uid_size / sizeof(stored_uids[0]);
        ESP_LOGI(TAG, "Found %d stored UIDs for deletion check", uid_count);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No UIDs stored yet, nothing to delete");
        nvs_close(my_handle);
        return;
    }
    else
    {
        ESP_LOGE(TAG, "Error reading stored UIDs: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return;
    }

    // Debug print the UID we're trying to delete
    ESP_LOGI(TAG, "Attempting to delete UID: ");
    for (int i = 0; i < uid_length; i++) {
        printf("%02X ", uid[i]);
    }
    printf("\n");

    // Search and remove the UID from the list if it exists
    bool found = false;
    for (size_t i = 0; i < uid_count; i++)
    {
        // Debug print for comparison
        ESP_LOGI(TAG, "Comparing with stored UID %d: ", i);
        for (int j = 0; j < uid_length; j++) {
            printf("%02X ", stored_uids[i][j]);
        }
        printf("\n");
        
        if (memcmp(stored_uids[i], uid, uid_length) == 0)
        {
            ESP_LOGI(TAG, "UID match found at position %d, deleting...", i);
            
            // Shift remaining UIDs to remove the found UID
            for (size_t j = i; j < uid_count - 1; j++)
            {
                memcpy(stored_uids[j], stored_uids[j + 1], UID_LENGTH);
            }
            uid_count--; // Decrease the UID count
            found = true;
            break;
        }
    }

    if (!found)
    {
        ESP_LOGI(TAG, "UID not found in storage.");
        nvs_close(my_handle);
        return;
    }
    else
    {
        ESP_LOGI(TAG, "UID deleted successfully, remaining UIDs: %d", uid_count);
    }

    // Store the updated list of UIDs back in NVS
    if (uid_count > 0) {
        err = nvs_set_blob(my_handle, "card_uids", stored_uids, uid_count * sizeof(stored_uids[0]));
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Error storing updated UIDs: %s", esp_err_to_name(err));
        }
    } else {
        // If no UIDs left, erase the entry completely
        err = nvs_erase_key(my_handle, "card_uids");
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error erasing UIDs key: %s", esp_err_to_name(err));
        }
        ESP_LOGI(TAG, "All UIDs removed from storage");
    }

    // Commit the changes
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error committing changes to NVS: %s", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(my_handle);
}

/*
 *----------------- store_uid_in_nvs -----------------
 * Store a UID from the NVS storage
 */
void store_uid_in_nvs(uint8_t *uid, uint8_t uid_length)
{
    esp_err_t err;
    nvs_handle_t my_handle;

    // Open NVS in read-write mode
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    // Read the existing list of UIDs
    uint8_t stored_uids[MAX_UIDS][UID_LENGTH];
    size_t uid_count = 0;
    size_t stored_uid_size = sizeof(stored_uids);

    err = nvs_get_blob(my_handle, "card_uids", stored_uids, &stored_uid_size);
    if (err == ESP_OK)
    {
        // Calculate how many UIDs are stored
        uid_count = stored_uid_size / sizeof(stored_uids[0]);
        ESP_LOGI(TAG, "Found %d existing UIDs", uid_count);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        // First UID to be stored - initialize uid_count to 0
        uid_count = 0;
        ESP_LOGI(TAG, "No existing UIDs found, creating new storage");
    }
    else
    {
        ESP_LOGE(TAG, "Error reading stored UIDs: %s", esp_err_to_name(err));
        nvs_close(my_handle);
        return;
    }

    // Add the new UID to the list
    if (uid_count < MAX_UIDS)
    {
        // Debug - print what we're storing
        ESP_LOGI(TAG, "Storing UID at position %d: ", uid_count);
        for (int i = 0; i < uid_length; i++) {
            printf("%02X ", uid[i]);
        }
        printf("\n");
        
        memcpy(stored_uids[uid_count], uid, uid_length);
        // Ensure remaining bytes are zeroed for smaller UIDs
        if (uid_length < UID_LENGTH) {
            memset(&stored_uids[uid_count][uid_length], 0, UID_LENGTH - uid_length);
        }
        uid_count++;
    }
    else
    {
        ESP_LOGW(TAG, "UID storage full, cannot add more UIDs");
        nvs_close(my_handle);
        return;
    }

    // Store the updated list of UIDs back in NVS
    err = nvs_set_blob(my_handle, "card_uids", stored_uids, uid_count * sizeof(stored_uids[0]));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error storing UIDs: %s", esp_err_to_name(err));
    }

    // Commit the changes
    err = nvs_commit(my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error committing changes to NVS: %s", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(my_handle);
}

/*
 *----------------- display_all_uids -----------------
 * Display all UIDs from the NVS storage
 */
void display_all_uids(void)
{
    esp_err_t err;
    nvs_handle_t my_handle;
    uint8_t stored_uids[MAX_UIDS][UID_LENGTH]; // Use consistent MAX_UIDS
    size_t stored_uid_size = sizeof(stored_uids);
    size_t uid_count = 0;

    // Open NVS in read mode
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    // Read the stored UIDs
    err = nvs_get_blob(my_handle, "card_uids", stored_uids, &stored_uid_size);
    if (err == ESP_OK)
    {
        // Calculate how many UIDs are stored
        uid_count = stored_uid_size / sizeof(stored_uids[0]);
        ESP_LOGI(TAG, "Stored UIDs: ");
        for (size_t i = 0; i < uid_count; i++)
        {
            printf("UID %zu: ", i + 1);
            for (int j = 0; j < 16; j++)
            {
                printf("%02X ", stored_uids[i][j]);
            }
            printf("\n");
        }
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No UIDs stored yet");
    }
    else
    {
        ESP_LOGE(TAG, "Error reading stored UIDs: %s", esp_err_to_name(err));
    }

    // Close NVS handle
    nvs_close(my_handle);
}

bool is_uid_in_nvs(uint8_t *uid, uint8_t uid_length)
{
    esp_err_t err;
    nvs_handle_t my_handle;
    uint8_t stored_uids[MAX_UIDS][UID_LENGTH]; // Use consistent array size
    size_t stored_uid_size = sizeof(stored_uids);
    size_t uid_count = 0;

    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return false;
    }

    err = nvs_get_blob(my_handle, "card_uids", stored_uids, &stored_uid_size);
    if (err == ESP_OK)
    {
        uid_count = stored_uid_size / sizeof(stored_uids[0]);
        ESP_LOGI(TAG, "Found %d stored UIDs", uid_count);

        // Check if UID is in stored list
        for (size_t i = 0; i < uid_count; i++)
        {
            // Debug output for comparison
            ESP_LOGI(TAG, "Comparing with stored UID %d: ", i);
            for (int j = 0; j < uid_length; j++) {
                printf("%02X ", stored_uids[i][j]);
            }
            printf("\n");
            
            if (memcmp(stored_uids[i], uid, uid_length) == 0)
            {
                ESP_LOGI(TAG, "UID match found!");
                nvs_close(my_handle);
                return true; // UID found
            }
        }
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "No UIDs stored yet");
    }
    else
    {
        ESP_LOGE(TAG, "Error reading stored UIDs: %s", esp_err_to_name(err));
    }

    nvs_close(my_handle);
    return false; // UID not found
}