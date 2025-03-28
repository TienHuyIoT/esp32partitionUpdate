#include "growPartition/growPartition.h"
#include <esp_ota_ops.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_system.h>
//#include <ESPmDNS.h>

constexpr char TAG[] = "PARTGROW";

#include "sdkconfig.h"

extern "C" void app_main(void) {
    GrowPartition growPartition;
    if (growPartition.grow ()) {
        ESP_LOGI (TAG, "Partition grown successfully!. Rebooting...");
        vTaskDelay (pdMS_TO_TICKS (1000)); // Wait for a second before rebooting
    }
    esp_ota_mark_app_invalid_rollback_and_reboot ();
    esp_restart ();
}


