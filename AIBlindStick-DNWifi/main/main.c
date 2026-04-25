#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "../components/distance_detect/include/distance_detect.h"
#include "../components/Camera/include/Camera.h"
#include "../components/Camera/include/Camstream.h"
#include "esp_psram.h"

void app_main(void)
{
    uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    if (psram_free > 0) {printf("PSRAM found! Free: %lu bytes\n", psram_free);
    } else {printf("PSRAM enabled in config but not detected at runtime!\n");}

    initcamera();
    stream();
    vTaskDelay(pdMS_TO_TICKS(50));
    takepic();


    distance();



    

    while (1)
    {

        vTaskDelay(portMAX_DELAY);
        


    }


}
