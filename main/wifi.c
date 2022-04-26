#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "context.h"
#include "smartconfig.h"

#define CONNECTED_BIT BIT0

static const char *TAG = "wifi";

static const TickType_t CONNECTION_TIMEOUT_TICKS = pdMS_TO_TICKS(10000);

static context_t *context;

static EventGroupHandle_t wifi_event_group;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event base: %s, id: %d", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t *event = (wifi_event_sta_connected_t *)event_data;
        ESP_LOGI(TAG, "Connected to %s", (const char *)event->ssid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Disconnected from %s, reason: %d", (const char *)event->ssid, event->reason);
        ESP_ERROR_CHECK(context_set_network_error(context, true));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "IPv4 address: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

static void wifi_connect(void)
{
    ESP_LOGI(TAG, "Connecting to %s...", (const char *)context->config.ssid);
    ESP_ERROR_CHECK(context_set_network_connected(context, false));
    ESP_ERROR_CHECK(context_set_network_error(context, false));

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, pdTRUE, pdTRUE, CONNECTION_TIMEOUT_TICKS);
    if (bits & CONNECTED_BIT) {
        ESP_ERROR_CHECK(context_set_network_connected(context, true));
        ESP_ERROR_CHECK(context_set_network_error(context, false));
    } else {
        ESP_LOGW(TAG, "Failed to connect to %s", (const char *)context->config.ssid);
        ESP_ERROR_CHECK(context_set_network_connected(context, false));
        ESP_ERROR_CHECK(context_set_network_error(context, true));
    }
}

static esp_err_t wifi_is_provisioned(bool *provisioned)
{
    if (!provisioned) {
        return ESP_ERR_INVALID_ARG;
    }

    *provisioned = false;

    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
        return ESP_FAIL;
    }

    if (strlen((const char *)wifi_cfg.sta.ssid)) {
        *provisioned = true;
        memcpy(context->config.ssid, wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password));
        memcpy(context->config.ssid, wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid));
        ESP_LOGI(TAG, "Saved ssid: %s", (const char *)wifi_cfg.sta.ssid);
        ESP_LOGI(TAG, "Saved password: %s", (const char *)wifi_cfg.sta.password);
        ESP_ERROR_CHECK(context_set_network_provisioned(context));
    }
    return ESP_OK;
}

static void wifi_init_sta(void)
{
    ESP_LOGI(TAG, "Initializing wifi...");
    esp_log_level_set("wifi", ESP_LOG_VERBOSE);
    ESP_ERROR_CHECK(context_set_network_connected(context, false));

    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void wifi_task(void *arg)
{
    context = (context_t *)arg;

    wifi_init_sta();

    bool provisioned;
    ESP_ERROR_CHECK(wifi_is_provisioned(&provisioned));
    if (!provisioned) {
        ESP_ERROR_CHECK(smartconfig_init(context));
    }
    while (true) {
        xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_NETWORK_PROVISIONED, pdTRUE, pdTRUE, portMAX_DELAY);
        ESP_ERROR_CHECK(esp_wifi_connect());
        while (true) {
            wifi_connect();
            xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_NETWORK_ERROR, pdTRUE, pdTRUE, portMAX_DELAY);
            vTaskDelay(pdMS_TO_TICKS(2000));
            ESP_ERROR_CHECK(esp_wifi_disconnect());
            ESP_ERROR_CHECK(esp_wifi_connect());
        }
    }
}

esp_err_t wifi_init(context_t *context)
{
    xTaskCreatePinnedToCore(wifi_task, "wifi", 4096, context, 2, NULL, tskNO_AFFINITY);
    return ESP_OK;
}
