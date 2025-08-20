#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/socket.h>
#include <netinet/in.h>  // Must come AFTER sys/socket.h

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int udp_sock;
    struct sockaddr_in pc_addr;
} proxy_sensor_params_t;

void proxy_sensor_task(void *pvParameters);

#ifdef __cplusplus
}
#endif


