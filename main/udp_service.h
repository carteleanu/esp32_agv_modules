#ifndef UDP_SERVICE_H
#define UDP_SERVICE_H

#include "esp_err.h"
#include "lwip/sockets.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize UDP socket for sending data
 *
 * @param ip   Destination IP (as string, e.g. "192.168.1.100")
 * @param port Destination port (e.g. 3333)
 * @return ESP_OK on success, ESP_FAIL otherwise
 */
esp_err_t udp_service_init(const char *ip, uint16_t port);

/**
 * @brief Send a message over UDP
 *
 * @param data Pointer to data buffer
 * @param len  Length of data
 * @return Number of bytes sent, or -1 on error
 */
int udp_service_send(const char *data, size_t len);

/**
 * @brief Close the UDP socket
 */
void udp_service_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // UDP_SERVICE_H
