#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "driver/gpio.h"

// Change this to your board's embedded LED GPIO
#define GREEN_LED_PIN 13
#define UDP_PORT 8888
#define BUFFER_SIZE 128

extern int udp_sock;             // Use your global socket from main
extern struct sockaddr_in pc_addr; // Use your global pc_addr from main

static const char *TAG = "UDP_LISTENER";

void udp_listener_task(void *pvParameters)
{
    // Initialize onboard LED as output
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << GREEN_LED_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    char buffer[BUFFER_SIZE];

    while (1)
    {
        if (udp_sock < 0) {
            ESP_LOGW(TAG, "UDP socket not ready, waiting...");
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);
        int len = recvfrom(udp_sock, buffer, BUFFER_SIZE - 1, 0,
                           (struct sockaddr *)&sender_addr, &addr_len);

        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        buffer[len] = '\0'; // Null-terminate the string
        ESP_LOGI(TAG, "Received UDP message: %s", buffer);

        if (strcmp(buffer, "LED_GREEN_ON") == 0) {
            ESP_LOGI(TAG, "Turning onboard LED ON for 2 seconds");
            gpio_set_level(GREEN_LED_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(2000));
            gpio_set_level(GREEN_LED_PIN, 0);
            ESP_LOGI(TAG, "LED OFF");
        }
    }
    vTaskDelete(NULL);
}
