#include "growPartition/growPartition.h"
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
#include "sdkconfig.h"

#define MAIN_CONSOLE_I(f_, ...)  printf("I [MAIN] Func[%s] line %u: " f_ "\r\n",  __func__, __LINE__, ##__VA_ARGS__)
#define MAIN_CONSOLE_E(f_, ...)  printf("E [MAIN] Func[%s] line %u: " f_ "\r\n",  __func__, __LINE__, ##__VA_ARGS__)

#if !defined(CONFIG_SPI_FLASH_DANGEROUS_WRITE_ALLOWED) || (CONFIG_SPI_FLASH_DANGEROUS_WRITE_ALLOWED == 0)
#error "CONFIG_SPI_FLASH_DANGEROUS_WRITE_ALLOWED must be enabled to use this code."
#endif

UpdatePartition gUpdatePartition(
    (const uint8_t *)NEW_PARTITION, 
    sizeof(NEW_PARTITION)
);

extern "C" void app_main(void) {
    if (gUpdatePartition.update(UpdatePartition::OTA_PART_ANY)) {
      MAIN_CONSOLE_I("Partition table update successfully!.");
      vTaskDelay (pdMS_TO_TICKS (1000)); // Wait for a second before rebooting
    } else {
        MAIN_CONSOLE_E("Failed / Canceled to update partition table.");
    }

    if (esp_ota_check_rollback_is_possible ()) {
        MAIN_CONSOLE_I("Rollback is possible. Rebooting...");
        esp_ota_mark_app_invalid_rollback_and_reboot ();
    } else {
        MAIN_CONSOLE_I("Rollback is not possible. Rebooting anyway...");
    }
    esp_restart ();
}


