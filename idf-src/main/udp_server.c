#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include <lwip/netdb.h>

#include "main.h"

static const char *TAG = "udp_server";

#define PORT 666

static const char *UNKNOWN_CMD = "unknown command";
static const char *SPEED_OK = "OK";

void udp_server_task(void *pvParameters) {
    char rx_buffer[128];
    char addr_str[128];
    int addr_family;
    int ip_protocol;

    while (1) {
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = bind(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err < 0) {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGI(TAG, "Socket binded");

        while (1) {
            ESP_LOGI(TAG, "Waiting for data");
            struct sockaddr_in6 sourceAddr; // Large enough for both IPv4 or IPv6
            socklen_t socklen = sizeof(sourceAddr);
            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&sourceAddr, &socklen);

            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
                break;
            } else {  // Data received
                // Get the sender's ip address as string
                if (sourceAddr.sin6_family == PF_INET) {
                    inet_ntoa_r(((struct sockaddr_in *)&sourceAddr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
                } else if (sourceAddr.sin6_family == PF_INET6) {
                    inet6_ntoa_r(sourceAddr.sin6_addr, addr_str, sizeof(addr_str) - 1);
                }

                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", rx_buffer);

                if (rx_buffer[0] == 'e') {  // echo
                    int err = sendto(sock, rx_buffer, len, 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
                    if (err < 0) { ESP_LOGE(TAG, "Error occured during sending: errno %d", errno); break; }
                } else if (rx_buffer[0] == 's') {  // something like "s 1023 -1023 1024" where the last one means the maximum value
                    int sp1, sp2, spf;
                    sscanf(rx_buffer + 1, "%d%d%d", &sp1, &sp2, &spf);
                    motor_update_speed(sp1 / ((float)spf), sp2 / ((float)spf));
                    int err = sendto(sock, SPEED_OK, strlen(SPEED_OK), 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
                    if (err < 0) { ESP_LOGE(TAG, "Error occured during sending: errno %d", errno); break; }
                } else {
                    int err = sendto(sock, UNKNOWN_CMD, strlen(UNKNOWN_CMD), 0, (struct sockaddr *)&sourceAddr, sizeof(sourceAddr));
                    if (err < 0) { ESP_LOGE(TAG, "Error occured during sending: errno %d", errno); break; }
                }
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }

    vTaskDelete(NULL);
}
