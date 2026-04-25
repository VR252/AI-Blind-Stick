#include <stdio.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_camera.h"


static const char *TAG = "CAM_STREAM";

#define WIFI_SSID ""
#define WIFI_PASS ""

/* ================= WIFI ================= */

static void wifi_init(void)
{

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    ESP_LOGI(TAG, "WiFi initialized");
}


/* ================= HTTP STREAM HANDLER ================= */

static esp_err_t stream_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");

    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            return ESP_FAIL;
        }

        char header[64];
        int header_len = snprintf(header, sizeof(header),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n",
            fb->len);

        httpd_resp_send_chunk(req, header, header_len);
        httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
        httpd_resp_send_chunk(req, "\r\n", 2);

        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(50)); // ~10–15 FPS
    }
}


/* ================= HTTP SERVER ================= */

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    httpd_start(&server, &config);

    httpd_uri_t stream_uri = {
        .uri = "/stream",
        .method = HTTP_GET,
        .handler = stream_handler,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(server, &stream_uri);
    ESP_LOGI(TAG, "HTTP server started");
}

void stream(void)
{
    //nvs_flash_init();
    wifi_init();

    //ESP_ERROR_CHECK(esp_camera_init(&camera_config));

    sensor_t * s = esp_camera_sensor_get();
    if (s != NULL) {
        // These are sensor-specific commands, not part of the static config
        s->set_vflip(s, 1);   // Flip vertically
        s->set_hmirror(s, 0); // Flip horizontally (usually needed with vflip)

        // Image Quality Tuning
        s->set_contrast(s, 1);       // Sharper blacks
        s->set_saturation(s, 1);     // Better colors
        s->set_sharpness(s, 2);      // Edge definition
        s->set_denoise(s, 1);        // Hardware noise reduction
        
        // Light Tuning
        s->set_whitebal(s, 1);
        s->set_ae_level(s, 0);       // Auto Exposure level (-2 to 2)
        s->set_exposure_ctrl(s, 1);       



        s->set_denoise(s, 1);
        s->set_gain_ctrl(s, 1);      // Enable gain control
        s->set_agc_gain(s, 0);       // Set manual gain low (0-30) or use set_gain_ceiling
        s->set_gainceiling(s, GAINCEILING_2X); // Options: 2X, 4X, 8X... Keep it low for less noise.
    }
    ESP_LOGI(TAG, "Camera initialized");

    start_webserver();
}
