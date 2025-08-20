#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include <netinet/in.h>

#define RFID_BUFFER_SIZE 64

// Event group identifiers
typedef enum {
    APP_EVENT = 0,
    APP_EVENT_HID_HOST
} app_event_group_t;

// App event queue structure
typedef struct {
    app_event_group_t event_group;
    struct {
        hid_host_device_handle_t handle;
        hid_host_driver_event_t event;
        void *arg;
    } hid_host_device;
} app_event_queue_t;

// Globals used across files
extern QueueHandle_t app_event_queue;
extern char rfid_buffer[RFID_BUFFER_SIZE];
extern int rfid_index;
extern int udp_sock;
extern struct sockaddr_in pc_addr;

// Public API (called from main.c)
void usb_lib_task(void *arg);
void gpio_isr_cb(void *arg);

void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg);

void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg);

void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg);
