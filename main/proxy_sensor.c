// proxy_sensor.c
#include "wifi_service.h"
#include "proxy_sensor.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <sys/socket.h>


extern EventGroupHandle_t wifi_event_group;

static const char *TAG = "proxy_sensor";

void proxy_sensor_task(void *pvParameters) {
    proxy_sensor_params_t *params = (proxy_sensor_params_t *) pvParameters;
    int udp_sock = params->udp_sock;
    struct sockaddr_in pc_addr = params->pc_addr;

    ESP_LOGI(TAG, "Proxy sensor task started");

    int simulated_distance = 0; // Simulated sensor value
    bool triggered_once_flag = false; // Ensure UDP sends only once per trigger

    while (1) {
        // Wait for Wi-Fi to be connected
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        // Simulate sensor reading
        simulated_distance = (simulated_distance + 1) % 100;

        // Approx. 2 feet trigger range
        if (simulated_distance >= 23 && simulated_distance <= 25) {
            if (!triggered_once_flag) {
                if (udp_sock >= 0) {
                    char sensor_data[64];
                    snprintf(sensor_data, sizeof(sensor_data), "TRIGGERED_DISTANCE: %d (approx 2ft)", simulated_distance);

                    ESP_LOGI(TAG, "Sending triggered sensor data: %s", sensor_data);
                    int err = sendto(udp_sock, sensor_data, strlen(sensor_data), 0,
                                     (struct sockaddr *)&pc_addr, sizeof(pc_addr));
                    if (err < 0) {
                        ESP_LOGE(TAG, "Failed to send triggered sensor data: errno %d", errno);
                    } else {
                        ESP_LOGI(TAG, "Sent %d bytes of triggered sensor data", err);
                    }
                } else {
                    ESP_LOGW(TAG, "UDP socket not ready for triggered sensor data.");
                }
                triggered_once_flag = true;
            }
        } else {
            triggered_once_flag = false;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    vTaskDelete(NULL);
}
