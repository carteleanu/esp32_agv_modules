#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_err.h"

// Event bit for successful Wi-Fi connection
#define WIFI_CONNECTED_BIT BIT0

// Expose event group so other tasks can wait on it
extern EventGroupHandle_t wifi_event_group;

// Initialize Wi-Fi (STA mode)
void wifi_service_init(void);

#endif // WIFI_SERVICE_H
