#include "udp_service.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "udp_service";
static int udp_sock = -1;
static struct sockaddr_in dest_addr;

esp_err_t udp_service_init(const char *ip, uint16_t port)
{
    if (udp_sock != -1) {
        ESP_LOGW(TAG, "UDP socket already initialized");
        return ESP_OK;
    }

    udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    dest_addr.sin_addr.s_addr = inet_addr(ip);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    ESP_LOGI(TAG, "UDP initialized to %s:%d", ip, port);
    return ESP_OK;
}

int udp_service_send(const char *data, size_t len)
{
    if (udp_sock < 0) {
        ESP_LOGE(TAG, "UDP socket not initialized");
        return -1;
    }

    int err = sendto(udp_sock, data, len, 0,
                     (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
    }
    return err;
}

void udp_service_deinit(void)
{
    if (udp_sock != -1) {
        shutdown(udp_sock, 0);
        close(udp_sock);
        udp_sock = -1;
        ESP_LOGI(TAG, "UDP socket closed");
    }
}
