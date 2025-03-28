# PartitionGrow

A tool to safely modify the ESP32 partition table via OTA (Over The Air).

## Description

This tool allows remote and safe modification of the ESP32 partition table through OTA updates. The process is designed to be secure with automatic rollback capability.

## How It Works

1. The tool is loaded via OTA
2. When executed, it checks if it's running from the "app1" partition
   - If running from app1: proceeds to apply partition table changes
   - If not running from app1: no changes are made
3. After the check and possible modification, the tool always returns to the previous firmware

## Important Notes

- If the original firmware is running from app1, no changes will be made
- For the process to work, a new OTA upload of the original firmware must be done so it runs from app0
- The tool includes security mechanisms and verification to prevent issues during modification
- It's crucial to maintain the partition names "app0" and "app1" unchanged in the CSV files

## Usage Process

1. Ensure the original firmware is in app0 (through an OTA update if necessary)
2. Load this tool via OTA
3. The tool will:
   - Verify if it's running from app1
   - Apply changes if safe to do so
   - Automatically return to the original firmware

## Partition Tables

The project includes two example partition tables:

1. `default.csv`: Default partition layout

   ```csv
   # Name,   Type, SubType, Offset,  Size, Flags
   nvs,      data, nvs,     0x9000,  0x5000,
   otadata,  data, ota,     0xe000,  0x2000,
   app0,     app,  ota_0,   0x10000, 0x140000,
   app1,     app,  ota_1,   0x150000,0x140000,
   spiffs,   data, spiffs,  0x290000,0x160000,
   coredump, data, coredump,0x3F0000,0x10000,
   ```

2. `app_1_5mb.csv`: Modified layout with larger app partitions

   ```csv
   # Name,   Type, SubType, Offset,  Size, Flags
   nvs,      data, nvs,     0x9000,  0x5000,
   otadata,  data, ota,     0xe000,  0x2000,
   app0,     app,  ota_0,   0x10000, 0x170000,
   app1,     app,  ota_1,   0x180000,0x170000,
   spiffs,   data, spiffs,  0x2F0000,0x100000,
   coredump, data, coredump,0x3F0000,0x10000,
   ```

## Generating Partition Binary

To generate the binary file from your CSV partition table:

1. Generate the binary:

   ```bash
   python $(IDF_PATH)/components/partition_table/gen_esp32part.py new_partition.bin new_partitions.csv
   ```

2. Convert binary to C header:

   ```bash
   xxd -C -n NEW_PARTITION -i new_partition.h new_partition.bin
   sed -i 's/unsigned char/const unsigned char/' new_partition.h
   ```

## Inspiration

This project is inspired by:

- [Article: Changing an ESP32 partition table over the air](https://medium.com/the-toit-take/changing-an-esp32-partition-table-over-the-air-276c86feeba8)
- [Original Code](https://gist.github.com/floitsch/ed2530caa613581057d8998dee0a911f)
