#include <stdio.h>
#include "Camera.h"
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "Camstream.h"
#include "../distance_detect/include/distance_detect.h"


#define GPIO_Button     GPIO_NUM_33 // pin 33 Read Button
#define GPIO_BUTTON_PIN_SEL (1ULL << GPIO_Button)
#define BUTTON_DEBOUNCE_MS 1000

#define CAM_PIN_PWDN    -1 //power down is not used
#define CAM_PIN_RESET   -1 //software reset will be performed
#define CAM_PIN_XCLK    21
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27

#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      19
#define CAM_PIN_D2      18
#define CAM_PIN_D1       5
#define CAM_PIN_D0       4
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

static const char *CamTAG = "Camera Module";

static camera_config_t camera_config = {
    .pin_pwdn  = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = 26,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,//YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_VGA,//QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 10, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1, //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY//CAMERA_GRAB_LATEST. Sets when buffers should be filled
};

static esp_err_t init_camera(void)
{
    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(CamTAG, "Camera Init Failed");
        return err;
    }

    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
        // These are sensor-specific commands, not part of the static config
        s->set_vflip(s, 1);   // Flip vertically
        s->set_hmirror(s, 1); // Flip horizontally (usually needed with vflip)

        // Image Quality Tuning
        s->set_contrast(s, 1);       // Sharper blacks
        s->set_saturation(s, 1);     // Better colors
        s->set_sharpness(s, 2);      // Edge definition
        s->set_denoise(s, 1);        // Hardware noise reduction
        
        // Light Tuning
        s->set_whitebal(s, 1);
        s->set_ae_level(s, 0);       // Auto Exposure level (-2 to 2)

        s->set_denoise(s, 1);
        s->set_gain_ctrl(s, 1);      // Enable gain control
        s->set_agc_gain(s, 0);       // Set manual gain low (0-30) or use set_gain_ceiling
        s->set_gainceiling(s, GAINCEILING_2X); // Options: 2X, 4X, 8X... Keep it low for less noise.
        return ESP_OK;
    }

    return err;
}

static QueueHandle_t button_evt_queue = NULL;

static void IRAM_ATTR button_isr_handler(void* arg)
{
        uint32_t gpio_num = (uint32_t)arg;
        gpio_intr_disable(gpio_num);
        xQueueSendFromISR(button_evt_queue, &gpio_num, NULL);

}

static void button_task(void* arg)
{
    uint32_t gpio_num;
    TickType_t last_press_tick = 0;
    TickType_t now;
    for (;;)
    {
        if (xQueueReceive(button_evt_queue, &gpio_num, pdMS_TO_TICKS(5000))) {
            manageintr(0);
            ESP_LOGI("BUTTON", "Button pressed on GPIO %lu", gpio_num);
            takepic();
            vTaskDelay(pdMS_TO_TICKS(50));
            ESP_LOGI("BUTTON", "Enabling Interrupts");
            gpio_intr_enable(gpio_num);
            manageintr(1);
        }
    }
}


void takepic(){

    ESP_LOGI(CamTAG, "Taking picture...");

    camera_fb_t *stale_fb = esp_camera_fb_get();
    if (stale_fb) esp_camera_fb_return(stale_fb);

    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic) {
        ESP_LOGE(CamTAG, "Failed to get frame buffer");
        return;
    }

    // use pic->buf to access the image
    ESP_LOGI(CamTAG, "Picture taken! Its size was: %zu bytes", pic->len);
    //bt_send_image(pic);
    esp_camera_fb_return(pic);
}


void initcamera(void)
{
    if(ESP_OK != init_camera()) {
        return;
    }

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = GPIO_BUTTON_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);
    //gpio_set_intr_type(GPIO_Button, GPIO_INTR_NEGEDGE);

    button_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
    gpio_isr_handler_add(GPIO_Button, button_isr_handler, (void*)GPIO_Button);
}
