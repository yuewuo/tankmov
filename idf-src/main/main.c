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

static const char *TAG = "main";

#define ESP_WIFI_SSID "SOAR_513"
#define ESP_WIFI_PASS "soar@ceca"
#define ESP_MAXIMUM_RETRY 20  // 如果密码错误要重连，最多尝试几次

static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
static int s_retry_num = 0;

static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        ESP_LOGI(TAG, "got ip:%s", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        ESP_LOGI(TAG, "connect to the AP fail\n");
        break;
    default:
        break;
    }
    return ESP_OK;
}

void wifi_init_sta() {
    s_wifi_event_group = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS
        },
    };

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s", ESP_WIFI_SSID, ESP_WIFI_PASS);
}

static void wait_for_ip() {
    ESP_LOGI(TAG, "Waiting for AP connection...");
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP");
}

void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGE(TAG, "nvs_flash_init err");
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    wait_for_ip();  // 阻塞等待已连接

    ESP_LOGI(TAG, "Connected, then initialize tankmov hardware");
    motor_gpio_initialize();

    ESP_LOGI(TAG, "Hardware initialization finished, creating daemon");
    xTaskCreate(udp_server_task, "udp_server", 4096, NULL, 5, NULL);
}
