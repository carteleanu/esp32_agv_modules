#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "wifi_service.h"
#include "udp_service.h"
#include "udp_listener.h"
#include "proxy_sensor.h"
#include "hid_host_app.h"

#define APP_QUIT_PIN GPIO_NUM_0
#define PC_IP_ADDR   "172.16.0.15"
#define PC_UDP_PORT  8888

static const char *TAG = "main";

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    wifi_service_init();

    // UDP init
    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp_sock < 0) ESP_LOGE(TAG,"Unable to create socket: errno %d", errno);
    else ESP_LOGI(TAG,"UDP socket created");

    memset(&pc_addr,0,sizeof(pc_addr));
    pc_addr.sin_family=AF_INET;
    pc_addr.sin_port=htons(PC_UDP_PORT);
    inet_pton(AF_INET,PC_IP_ADDR,&pc_addr.sin_addr);

    ESP_ERROR_CHECK(udp_service_init(PC_IP_ADDR,PC_UDP_PORT));
    udp_service_send(rfid_buffer,strlen(rfid_buffer));

    xTaskCreate(udp_listener_task,"udp_listener_task",4096,NULL,5,NULL);

    proxy_sensor_params_t proxy_params={.udp_sock=udp_sock,.pc_addr=pc_addr};
    xTaskCreate(proxy_sensor_task,"proxy_sensor_task",4096,&proxy_params,5,NULL);

    // HID Host setup
    const gpio_config_t input_pin={.pin_bit_mask=BIT64(APP_QUIT_PIN),.mode=GPIO_MODE_INPUT,
                                   .pull_up_en=GPIO_PULLUP_ENABLE,.intr_type=GPIO_INTR_NEGEDGE};
    ESP_ERROR_CHECK(gpio_config(&input_pin));
    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(APP_QUIT_PIN,gpio_isr_cb,NULL));

    BaseType_t task_created=xTaskCreatePinnedToCore(usb_lib_task,"usb_events",4096,
                                                    xTaskGetCurrentTaskHandle(),2,NULL,0);
    assert(task_created==pdTRUE);
    ulTaskNotifyTake(false,1000/portTICK_PERIOD_MS);

    const hid_host_driver_config_t hid_host_driver_config={
        .create_background_task=true,.task_priority=5,.stack_size=4096,.core_id=0,
        .callback=hid_host_device_callback,.callback_arg=NULL};
    ESP_ERROR_CHECK(hid_host_install(&hid_host_driver_config));

    app_event_queue=xQueueCreate(10,sizeof(app_event_queue_t));
    if (!app_event_queue) ESP_LOGE(TAG,"Failed to create app_event_queue");

    ESP_LOGI(TAG,"Waiting for HID Device...");

    app_event_queue_t evt_queue;
    while (1) {
        if (xQueueReceive(app_event_queue,&evt_queue,portMAX_DELAY)) {
            if (APP_EVENT==evt_queue.event_group) {
                usb_host_lib_info_t lib_info;
                ESP_ERROR_CHECK(usb_host_lib_info(&lib_info));
                if (lib_info.num_devices==0) break;
                else ESP_LOGW(TAG,"Remove USB devices and press button again.");
            }
            if (APP_EVENT_HID_HOST==evt_queue.event_group) {
                hid_host_device_event(evt_queue.hid_host_device.handle,
                                      evt_queue.hid_host_device.event,
                                      evt_queue.hid_host_device.arg);
            }
        }
    }

    ESP_LOGI(TAG,"HID Driver uninstall");
    ESP_ERROR_CHECK(hid_host_uninstall());
    gpio_isr_handler_remove(APP_QUIT_PIN);

    if (app_event_queue){xQueueReset(app_event_queue);vQueueDelete(app_event_queue);app_event_queue=NULL;}
    if (udp_sock>=0){ESP_LOGI(TAG,"Closing UDP socket");close(udp_sock);udp_sock=-1;}

    ESP_LOGI(TAG,"Application finished.");
}
