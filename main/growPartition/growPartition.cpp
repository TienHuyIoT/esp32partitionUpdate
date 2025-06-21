// Copyright (C) 2023 Toitware ApS.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
// OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <stdio.h>
#include "esp_log.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include "freertos/task.h"
#include <esp_ota_ops.h>
#include <esp_spi_flash.h>
#include <esp_partition.h>
#include <esp_system.h>
#include "growPartition.h"

#define PARTITION_UPDATE_CONSOLE_I(f_, ...)  printf("I [PARTITION_UPDATE] Func[%s] line %u: " f_ "\r\n",  __func__, __LINE__, ##__VA_ARGS__)
#define PARTITION_UPDATE_CONSOLE_E(f_, ...)  printf("E [PARTITION_UPDATE] Func[%s] line %u: " f_ "\r\n",  __func__, __LINE__, ##__VA_ARGS__)

esp_err_t UpdatePartition::replace_partition_table() {
    // Copy the partition table to DRAM. This is a requirement of spi_flash_write.
    uint8_t* new_partition_table = (uint8_t*)malloc (PARTITION_TABLE_SIZE);
    if (new_partition_table == NULL) {
        PARTITION_UPDATE_CONSOLE_E("Failed to allocate memory for partition table.");
        return ESP_FAIL;
    }

    uint8_t* read_partition_table = (uint8_t*)malloc (_new_partition_len);
    if (read_partition_table == NULL) {
        PARTITION_UPDATE_CONSOLE_E("Failed to allocate memory for reading partition table.");
        free(new_partition_table);
        return ESP_FAIL;
    }

    memset (new_partition_table, 0xFF, PARTITION_TABLE_SIZE);
    memcpy(new_partition_table, _new_partition, _new_partition_len);

    if (new_partition_table[0] != 0xAA || new_partition_table[1] != 0x50) {
        free(new_partition_table);
        PARTITION_UPDATE_CONSOLE_E("Incorrect magic number: 0x%x%x", new_partition_table[0], new_partition_table[1]);
        return ESP_FAIL;
    }

    esp_err_t err = ESP_FAIL;
    for (int attempts = 1; attempts <= RETRY_LIMIT; attempts++) {
        PARTITION_UPDATE_CONSOLE_I ("Attempt %d of %d:", attempts, RETRY_LIMIT);
        PARTITION_UPDATE_CONSOLE_I ("Erasing partition table...");
        // Erase the partition table region.
        // Note: The partition table must be aligned to 4k, so we use PARTITION_TABLE_ALIGNED_SIZE.
        // The address of the partition table is PARTITION_TABLE_ADDRESS, which is 0x8000.
        // The partition table size is PARTITION_TABLE_SIZE, which is 0xC00.
        // The aligned size is PARTITION_TABLE_ALIGNED_SIZE, which is 0x1000.
        // Note: The partition table is stored at address 0x8000, which is the default address for the partition table.
        // Note: The partition table is aligned to 4k, so we need to erase the entire 4k region.
        err = esp_flash_erase_region(esp_flash_default_chip, PARTITION_TABLE_ADDRESS, PARTITION_TABLE_ALIGNED_SIZE);
        if (err != ESP_OK) {
            PARTITION_UPDATE_CONSOLE_E("Failed to erase partition table: 0x%x", err);
            vTaskDelay(pdMS_TO_TICKS(RETRY_WAIT));
            continue;
        }

        err = esp_flash_write(esp_flash_default_chip, new_partition_table, PARTITION_TABLE_ADDRESS, PARTITION_TABLE_ALIGNED_SIZE);
        if (err != ESP_OK) {
            PARTITION_UPDATE_CONSOLE_E("Failed to write partition table: 0x%x", err);
            vTaskDelay(pdMS_TO_TICKS(RETRY_WAIT));
            continue;
        }

        err = esp_flash_read(esp_flash_default_chip, read_partition_table, PARTITION_TABLE_ADDRESS, PARTITION_TABLE_ALIGNED_SIZE);
        if (err != ESP_OK) {
            PARTITION_UPDATE_CONSOLE_E("Failed to write partition table: 0x%x", err);
            vTaskDelay(pdMS_TO_TICKS(RETRY_WAIT));
            continue;
        }

        // Verify that the partition table was written correctly.
        if (memcmp(new_partition_table, read_partition_table, _new_partition_len) != 0) {
            PARTITION_UPDATE_CONSOLE_E("Partition table verification failed.");
            vTaskDelay(pdMS_TO_TICKS(RETRY_WAIT));
            continue;
        }
        PARTITION_UPDATE_CONSOLE_I("Partition table written successfully.");
        err = ESP_OK;
        // If we reach here, the partition table was written successfully.
        break;
    }
    free(new_partition_table);
    PARTITION_UPDATE_CONSOLE_I("Wrote new partition table");
    return err;
}

uint8_t UpdatePartition::running_from_ota() {
    const esp_partition_t* running_partition = esp_ota_get_running_partition();
    if (strcmp(running_partition->label, "app1") == 0) {
        PARTITION_UPDATE_CONSOLE_I("Running from app1 partition");
        return OTA_PART_1;
    } else if (strcmp(running_partition->label, "app0") == 0) {
        PARTITION_UPDATE_CONSOLE_I("Running from app0 partition");
        return OTA_PART_0;
    } else {
        PARTITION_UPDATE_CONSOLE_E("Running from unknown partition: %s", running_partition->label);
        return OTA_PART_ANY;
    }
    // Note: This function assumes that the partition labels are "app0" and "app1".
    // If the labels are different, you may need to adjust the comparisons accordingly.
    // If the running partition is not one of the OTA partitions, return OTA_PART_ANY.
}

bool UpdatePartition::invalid_or_already_written() {
    // Check if the partition table has already been written.
    uint8_t buffer[256];
    for (int offset = 0; offset < _new_partition_len; offset += 256) {
        esp_err_t err = esp_flash_read(esp_flash_default_chip, buffer, PARTITION_TABLE_ADDRESS + offset, 256);
        if (err != ESP_OK) {
            PARTITION_UPDATE_CONSOLE_E ("Failed to read partition table: 0x%x", err);
            return true;
        }
        if (offset == 0 && (buffer[0] != 0xAA || buffer[1] != 0x50)) {
            PARTITION_UPDATE_CONSOLE_E ("Invalid existing partition table. Maybe bad offset.");
            return true;
        }
        for (int i = 0; i < 256; i++) {
            if (buffer[i] != _new_partition[offset + i]) {
                PARTITION_UPDATE_CONSOLE_E ("Partition table differs at %d", offset + i);
                return false; // The partition table is not the same as the new one.
            }
        }
    }
    return true;
}

bool UpdatePartition::update(uint8_t ota_partition) {
    PARTITION_UPDATE_CONSOLE_I ("Running from partition %s", esp_ota_get_running_partition ()->label);
    if (invalid_or_already_written()) {
        PARTITION_UPDATE_CONSOLE_E("Partition table already written or valid. No need to update.");
        return false;
    }

    if (ota_partition != OTA_PART_ANY && running_from_ota() != ota_partition) {
        PARTITION_UPDATE_CONSOLE_E("Can't update: running partition needs to be app%d.", ota_partition);
        return false;
    }

    if (replace_partition_table() != ESP_OK) {
        PARTITION_UPDATE_CONSOLE_E("Failed to replace partition table");
        return false;
    }
    return true;
}
