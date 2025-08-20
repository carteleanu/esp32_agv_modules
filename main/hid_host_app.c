#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "esp_log.h"
#include "usb/usb_host.h"
#include "usb/hid_host.h"
#include "usb/hid_usage_keyboard.h"
#include "usb/hid_usage_mouse.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"
#include "hid_host_app.h"

static const char *TAG = "hid_host_app";

// Global definitions
QueueHandle_t app_event_queue = NULL;
char rfid_buffer[RFID_BUFFER_SIZE];
int rfid_index = 0;
int udp_sock = -1;
struct sockaddr_in pc_addr;

// Protocol string names
static const char *hid_proto_name_str[] = {
    "NONE",
    "KEYBOARD",
    "MOUSE"
};

/* ------------ Keyboard helpers ------------ */

typedef struct {
    enum key_state {
        KEY_STATE_PRESSED = 0x00,
        KEY_STATE_RELEASED = 0x01
    } state;
    uint8_t modifier;
    uint8_t key_code;
} key_event_t;

#define KEYBOARD_ENTER_MAIN_CHAR '\r'
#define KEYBOARD_ENTER_LF_EXTEND 1

// ASCII mapping table
const uint8_t keycode2ascii[57][2] = {
    {0, 0},{0, 0},{0, 0},{0, 0},
    {'a','A'},{'b','B'},{'c','C'},{'d','D'},{'e','E'},
    {'f','F'},{'g','G'},{'h','H'},{'i','I'},{'j','J'},
    {'k','K'},{'l','L'},{'m','M'},{'n','N'},{'o','O'},
    {'p','P'},{'q','Q'},{'r','R'},{'s','S'},{'t','T'},
    {'u','U'},{'v','V'},{'w','W'},{'x','X'},{'y','Y'},
    {'z','Z'},{'1','!'},{'2','@'},{'3','#'},{'4','$'},
    {'5','%'},{'6','^'},{'7','&'},{'8','*'},{'9','('},
    {'0',')'},{KEYBOARD_ENTER_MAIN_CHAR,KEYBOARD_ENTER_MAIN_CHAR},
    {0,0},{'\b',0},{0,0},{' ',' '},{'-','_'},{'=','+'},
    {'[','{'},{']','}'},{'\\','|'},{'\\','|'},
    {';',':'},{'\'','\"'},{'`','~'},{',','<'},{'.','>'},{'/','?'}
};

static void hid_print_new_device_report_header(hid_protocol_t proto) {
    static hid_protocol_t prev_proto_output = -1;
    if (prev_proto_output != proto) {
        prev_proto_output = proto;
        printf("\r\n");
        if (proto == HID_PROTOCOL_MOUSE) printf("Mouse\r\n");
        else if (proto == HID_PROTOCOL_KEYBOARD) printf("Keyboard\r\n");
        else printf("Generic\r\n");
        fflush(stdout);
    }
}

static inline bool hid_keyboard_is_modifier_shift(uint8_t modifier) {
    return ((modifier & HID_LEFT_SHIFT) || (modifier & HID_RIGHT_SHIFT));
}

static inline bool hid_keyboard_get_char(uint8_t modifier, uint8_t key_code, unsigned char *key_char) {
    uint8_t mod = (hid_keyboard_is_modifier_shift(modifier)) ? 1 : 0;
    if ((key_code >= HID_KEY_A) && (key_code <= HID_KEY_SLASH)) {
        *key_char = keycode2ascii[key_code][mod];
    } else return false;
    return true;
}

static inline void hid_keyboard_print_char(unsigned int key_char) {
    if (!!key_char) {
        putchar(key_char);
#if (KEYBOARD_ENTER_LF_EXTEND)
        if (KEYBOARD_ENTER_MAIN_CHAR == key_char) putchar('\n');
#endif
        fflush(stdout);
    }
}

static void key_event_callback(key_event_t *key_event) {
    unsigned char key_char;
    hid_print_new_device_report_header(HID_PROTOCOL_KEYBOARD);

    if (KEY_STATE_PRESSED == key_event->state) {
        if (hid_keyboard_get_char(key_event->modifier, key_event->key_code, &key_char)) {
            hid_keyboard_print_char(key_char);
            if (key_char == '\r') {
                rfid_buffer[rfid_index] = '\0';
                if (udp_sock >= 0 && rfid_index > 0) {
                    ESP_LOGI(TAG, "Sending RFID tag: %s", rfid_buffer);
                    int sent = sendto(udp_sock, rfid_buffer, strlen(rfid_buffer), 0,
                                      (struct sockaddr *)&pc_addr, sizeof(pc_addr));
                    if (sent < 0) ESP_LOGE(TAG, "UDP send failed: errno %d", errno);
                    else ESP_LOGI(TAG, "Sent %d bytes via UDP", sent);
                }
                rfid_index = 0;
                memset(rfid_buffer, 0, RFID_BUFFER_SIZE);
            } else {
                if (rfid_index < RFID_BUFFER_SIZE - 1)
                    rfid_buffer[rfid_index++] = key_char;
                else ESP_LOGW(TAG, "RFID buffer full, discarding char");
            }
        }
    }
}

static inline bool key_found(const uint8_t *src, uint8_t key, unsigned int length) {
    for (unsigned int i=0;i<length;i++) if (src[i] == key) return true;
    return false;
}

/* ------------ HID Callbacks ------------ */

static void hid_host_keyboard_report_callback(const uint8_t *data, const int length) {
    hid_keyboard_input_report_boot_t *kb_report = (hid_keyboard_input_report_boot_t *)data;
    if (length < sizeof(hid_keyboard_input_report_boot_t)) return;

    static uint8_t prev_keys[HID_KEYBOARD_KEY_MAX] = {0};
    key_event_t key_event;

    for (int i = 0; i < HID_KEYBOARD_KEY_MAX; i++) {
        if (prev_keys[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(kb_report->key, prev_keys[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = prev_keys[i];
            key_event.modifier = 0;
            key_event.state = KEY_STATE_RELEASED;
            key_event_callback(&key_event);
        }

        if (kb_report->key[i] > HID_KEY_ERROR_UNDEFINED &&
            !key_found(prev_keys, kb_report->key[i], HID_KEYBOARD_KEY_MAX)) {
            key_event.key_code = kb_report->key[i];
            key_event.modifier = kb_report->modifier.val;
            key_event.state = KEY_STATE_PRESSED;
            key_event_callback(&key_event);
        }
    }
    memcpy(prev_keys, &kb_report->key, HID_KEYBOARD_KEY_MAX);
}

static void hid_host_mouse_report_callback(const uint8_t *data, const int length) {
    hid_mouse_input_report_boot_t *mouse_report = (hid_mouse_input_report_boot_t *)data;
    if (length < sizeof(hid_mouse_input_report_boot_t)) return;
    static int x_pos=0,y_pos=0;
    x_pos += mouse_report->x_displacement;
    y_pos += mouse_report->y_displacement;
    hid_print_new_device_report_header(HID_PROTOCOL_MOUSE);
    printf("X:%06d Y:%06d |%c|%c|\r", x_pos, y_pos,
           (mouse_report->buttons.button1?'o':' '),
           (mouse_report->buttons.button2?'o':' '));
    fflush(stdout);
}

static void hid_host_generic_report_callback(const uint8_t *data, const int length) {
    hid_print_new_device_report_header(HID_PROTOCOL_NONE);
    for (int i=0;i<length;i++) printf("%02X", data[i]);
    putchar('\r');
}

void hid_host_interface_callback(hid_host_device_handle_t hid_device_handle,
                                 const hid_host_interface_event_t event,
                                 void *arg) {
    uint8_t data[64]={0}; size_t data_length=0;
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle,&dev_params));

    switch(event){
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        ESP_ERROR_CHECK(hid_host_device_get_raw_input_report_data(
                            hid_device_handle,data,64,&data_length));
        if (HID_SUBCLASS_BOOT_INTERFACE==dev_params.sub_class) {
            if (HID_PROTOCOL_KEYBOARD==dev_params.proto)
                hid_host_keyboard_report_callback(data,data_length);
            else if (HID_PROTOCOL_MOUSE==dev_params.proto)
                hid_host_mouse_report_callback(data,data_length);
        } else hid_host_generic_report_callback(data,data_length);
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG,"HID Device '%s' DISCONNECTED",hid_proto_name_str[dev_params.proto]);
        ESP_ERROR_CHECK(hid_host_device_close(hid_device_handle));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGI(TAG,"HID Device '%s' TRANSFER_ERROR",hid_proto_name_str[dev_params.proto]);
        break;
    default:
        ESP_LOGE(TAG,"HID Device '%s' Unhandled event",hid_proto_name_str[dev_params.proto]);
        break;
    }
}

void hid_host_device_event(hid_host_device_handle_t hid_device_handle,
                           const hid_host_driver_event_t event,
                           void *arg) {
    hid_host_dev_params_t dev_params;
    ESP_ERROR_CHECK(hid_host_device_get_params(hid_device_handle,&dev_params));
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        ESP_LOGI(TAG,"HID Device '%s' CONNECTED",hid_proto_name_str[dev_params.proto]);
        const hid_host_device_config_t dev_config = {
            .callback = hid_host_interface_callback,
            .callback_arg = NULL
        };
        ESP_ERROR_CHECK(hid_host_device_open(hid_device_handle,&dev_config));
        if (HID_SUBCLASS_BOOT_INTERFACE==dev_params.sub_class) {
            ESP_ERROR_CHECK(hid_class_request_set_protocol(hid_device_handle,HID_REPORT_PROTOCOL_BOOT));
            if (HID_PROTOCOL_KEYBOARD==dev_params.proto)
                ESP_ERROR_CHECK(hid_class_request_set_idle(hid_device_handle,0,0));
        }
        ESP_ERROR_CHECK(hid_host_device_start(hid_device_handle));
    }
}

void hid_host_device_callback(hid_host_device_handle_t hid_device_handle,
                              const hid_host_driver_event_t event,
                              void *arg) {
    const app_event_queue_t evt_queue = {
        .event_group=APP_EVENT_HID_HOST,
        .hid_host_device.handle=hid_device_handle,
        .hid_host_device.event=event,
        .hid_host_device.arg=arg
    };
    if (app_event_queue) xQueueSend(app_event_queue,&evt_queue,0);
}

/* ------------ USB + ISR ------------ */

void usb_lib_task(void *arg) {
    const usb_host_config_t host_config = {.skip_phy_setup=false,.intr_flags=ESP_INTR_FLAG_LEVEL1};
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive(arg);
    while (true) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY,&event_flags);
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            ESP_ERROR_CHECK(usb_host_device_free_all());
            break;
        }
    }
    ESP_LOGI(TAG,"USB shutdown");
    vTaskDelay(10/portTICK_PERIOD_MS);
    ESP_ERROR_CHECK(usb_host_uninstall());
    vTaskDelete(NULL);
}

void gpio_isr_cb(void *arg) {
    BaseType_t xTaskWoken=pdFALSE;
    const app_event_queue_t evt_queue={.event_group=APP_EVENT};
    if (app_event_queue) xQueueSendFromISR(app_event_queue,&evt_queue,&xTaskWoken);
    if (xTaskWoken==pdTRUE) portYIELD_FROM_ISR();
}
