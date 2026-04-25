#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <iso646.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "driver/gptimer.h"
#include "esp_log.h"



#define GPIO_OUTPUT_IO_0    GPIO_NUM_32 // pin 32 output trigger
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_OUTPUT_IO_0)

#define GPIO_INPUT_IO_1     GPIO_NUM_13 // pin 13 input on echo
#define GPIO_INPUT_PIN_SEL  (1ULL << GPIO_INPUT_IO_1)

#define GPIO_OUTPUT_MOTOR_1     GPIO_NUM_14 // pin 13 input on echo
#define GPIO_OUTPUT_MOTOR_SEL_1  (1ULL << GPIO_OUTPUT_MOTOR_1)


#define ESP_INTR_FLAG_DEFAULT 0

#define maxdistance 150
#define mindistance 20

static QueueHandle_t gpio_evt_queue = NULL;
static const char *TimeTAG = "TIMER_EXAMPLE";

volatile uint32_t start_count;
int motordelay = 0;
int motorvalue = 0;
int motorcount = 0;
volatile uint32_t ret_count;
float distance_cm;


static gptimer_handle_t gptimer = NULL;
static TaskHandle_t timer_task_handle = NULL;
static TaskHandle_t buzzmotor_task_handle = NULL;



/* ===================== Get clock count ===================== */
static inline uint32_t get_ccount(void)
{
    uint32_t ccount;
    __asm__ __volatile__("rsr %0, ccount" : "=a"(ccount));
    return ccount;
}



/* ===================== Motor Control ===================== */


void buzzmotor(void *arg)
{
    while (1) {
        // Sleep until explicitly woken
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        gpio_set_level(GPIO_OUTPUT_MOTOR_1, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(GPIO_OUTPUT_MOTOR_1, 0);

    }
}


/* ===================== GPIO INTERRUPT INITIALIZATION ===================== */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    if(gpio_get_level(GPIO_INPUT_IO_1) == 1) {
        ret_count = 0;
        start_count = get_ccount();   // Rising edge
    } else {
        ret_count = get_ccount() - start_count;     // Falling edge      
        uint32_t gpio_num = (uint32_t) arg;
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }

}

/* ===================== GPIO INTERRUPT HANDLER ===================== */
static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            float time_us = (float)(ret_count) / 160.0f;
            distance_cm = time_us / 58.0f;
            printf("Distance: %.2f cm\n", distance_cm);

            if(distance_cm < maxdistance){
                if(distance_cm > 20){
                    if(distance_cm < 30){
                        motordelay = 2;
                    }else if(distance_cm < 40){
                        motordelay = 3;
                    }else if(distance_cm < 50){
                        motordelay = 4;
                    }else if(distance_cm < 60){
                        motordelay = 5;
                    }else if(distance_cm < 70){
                        motordelay = 6;
                    }else if(distance_cm < 80){
                        motordelay = 7;
                    }else if(distance_cm < 90){
                        motordelay = 8;
                    }else if(distance_cm < 100){
                        motordelay = 9;
                    }else{
                        motordelay = 10;
                    }
                    if (motorcount % motordelay == 0){
                        xTaskNotifyGive(buzzmotor_task_handle);
                    }
                }else{
                    xTaskNotifyGive(buzzmotor_task_handle);
                }
            motorcount++;
            }
        }
    }
}




/* ===================== TIMER ISR CALLBACK ===================== */
static bool IRAM_ATTR timer_isr_callback(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_data)
{
    BaseType_t high_task_wakeup = pdFALSE;

    /* Notify task */
    xTaskNotifyFromISR(timer_task_handle, 0, eNoAction, &high_task_wakeup);

    return (high_task_wakeup == pdTRUE);
}

/* ===================== TIMER INITIALIZATION ===================== */
static void timer_init(void)
{
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000, // 1 MHz → 1 µs resolution
    };

    ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &gptimer));

    gptimer_event_callbacks_t callbacks = {
        .on_alarm = timer_isr_callback,
    };
    ESP_ERROR_CHECK(gptimer_register_event_callbacks(gptimer, &callbacks, NULL));

    gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = 100000, // 10 ms (100 Hz)
        .flags.auto_reload_on_alarm = true,
    };
    ESP_ERROR_CHECK(gptimer_set_alarm_action(gptimer, &alarm_config));

    ESP_ERROR_CHECK(gptimer_enable(gptimer));
    ESP_ERROR_CHECK(gptimer_start(gptimer));

    ESP_LOGI(TimeTAG, "Hardware timer started (10 ms period)");
}

/* ===================== TASK HANDLER ===================== */
static void timer_task(void *arg)
{
    ESP_LOGI(TimeTAG, "Timer task started");

    while (1) {
        /* Wait for timer interrupt notification */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        gpio_set_level(GPIO_OUTPUT_IO_0, 1);
        esp_rom_delay_us(10);
        gpio_set_level(GPIO_OUTPUT_IO_0, 0);
        /* Periodic work here */
        //ESP_LOGI(TimeTAG, "Timer tick");
    }
}


void manageintr(int action){
    if (action == 0){
        gptimer_stop(gptimer);
        gpio_intr_disable(GPIO_INPUT_IO_1);
    } else {
        gptimer_start(gptimer);
        gpio_intr_enable(GPIO_INPUT_IO_1);
    }
}


void distance(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //gpio_set_intr_type(GPIO_INPUT_IO_1, GPIO_INTR_ANYEDGE);


    //Motor GPIO pin12
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = GPIO_OUTPUT_MOTOR_SEL_1;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);

    gpio_set_level(GPIO_OUTPUT_MOTOR_1, 0);



    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "Echo Signal", 2048, NULL, 10, NULL);

    //gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void*) GPIO_INPUT_IO_1);

    printf("Minimum free heap size: %"PRIu32" bytes\n", esp_get_minimum_free_heap_size());


    xTaskCreate(
        buzzmotor,                 // task function
        "buzzmotor",
        2048,                      // stack size
        NULL,
        5,                         // priority
        &buzzmotor_task_handle     // save handle
    );


    xTaskCreatePinnedToCore(
        timer_task,
        "timer_task",
        2048,
        NULL,
        10,
        &timer_task_handle,
        1
    );
    /* Initialize and start hardware timer */
    timer_init();

    

}
