/*
 * Heap leak demo for Wokwi long-run testing.
 *
 * Every button press (or line received on UART0) is one "request". The request handler leaks its
 * 1 KB response buffer on the cache-hit path, which is taken on every 4th request.
 *
 * The firmware has no heap instrumentation: wokwi-cli reads the ESP-IDF heap counters from the
 * simulation (--heap-min-free, --heap-csv). CONFIG_DEMO_HEAP_LOG adds a task that logs the heap and
 * prints "HEAP LOW", to watch the leak on a real board.
 */
#include <stdio.h>
#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#if CONFIG_DEMO_HEAP_LOG
#include <stdbool.h>
#include "esp_heap_caps.h"
#endif

#define BUTTON_GPIO       GPIO_NUM_4
#define RESPONSE_SIZE     1024
#define CONSOLE_UART      UART_NUM_0
#define PROGRESS_INTERVAL 50

static const char *TAG = "app";

static QueueHandle_t request_queue;
static char *cached_response;

static void handle_request(uint32_t id)
{
    char *response = malloc(RESPONSE_SIZE);
    if (response == NULL) {
        ESP_LOGE(TAG, "out of memory at request %lu", (unsigned long)id);
        return;
    }

    if (cached_response != NULL && id % 4 == 0) {
        /* Cache hit: the cached response is still fresh, serve it again. */
        return; /* BUG: `response` leaks here */
    }

    snprintf(response, RESPONSE_SIZE, "{\"id\":%lu,\"temp\":%lu}", (unsigned long)id,
             (unsigned long)(20 + id % 5));
    free(cached_response);
    cached_response = response;
}

static void request_task(void *arg)
{
    uint32_t id = 0;
    for (;;) {
        uint8_t source;
        xQueueReceive(request_queue, &source, portMAX_DELAY);
        handle_request(++id);
        if (id % PROGRESS_INTERVAL == 0) {
            ESP_LOGI(TAG, "handled %lu requests", (unsigned long)id);
        }
    }
}

static void IRAM_ATTR button_isr(void *arg)
{
    uint8_t source = 'b';
    xQueueSendFromISR(request_queue, &source, NULL);
}

static void uart_task(void *arg)
{
    uint8_t c;
    for (;;) {
        if (uart_read_bytes(CONSOLE_UART, &c, 1, portMAX_DELAY) == 1 && c == '\n') {
            xQueueSend(request_queue, &c, portMAX_DELAY);
        }
    }
}

#if CONFIG_DEMO_HEAP_LOG
static void heap_log_task(void *arg)
{
    const size_t baseline = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    const size_t budget = CONFIG_DEMO_HEAP_BUDGET_KB * 1024;
    bool low_reported = false;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_DEMO_HEAP_REPORT_MS));
        size_t free_bytes = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
        ESP_LOGI("heap", "free=%u min=%u", (unsigned)free_bytes,
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT));
        if (!low_reported && free_bytes + budget < baseline) {
            ESP_LOGE("heap", "HEAP LOW: free=%u lost=%u budget=%u", (unsigned)free_bytes,
                     (unsigned)(baseline - free_bytes), (unsigned)budget);
            low_reported = true;
        }
    }
}
#endif

void app_main(void)
{
    request_queue = xQueueCreate(64, sizeof(uint8_t));

    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&button));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL));

    ESP_ERROR_CHECK(uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0));

    xTaskCreate(request_task, "request", 4096, NULL, 5, NULL);
    xTaskCreate(uart_task, "uart", 2048, NULL, 4, NULL);
#if CONFIG_DEMO_HEAP_LOG
    xTaskCreate(heap_log_task, "heap", 3072, NULL, 3, NULL);
#endif

    ESP_LOGI(TAG, "ready");
}
