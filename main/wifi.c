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

#define WIFI_CONNECTED_BIT BIT0

typedef struct {
    context_t *context;
    const char *ssid;
    const char *password;
} args_t;

static const char *TAG = "wifi";

static const TickType_t CONNECTION_TIMEOUT_TICKS = pdMS_TO_TICKS(10000);

static EventGroupHandle_t wifi_event_group;
static args_t args = {0};

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event base: %s, id: %d", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *evt = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "Disconnected from %s, reason: %d", evt->ssid, evt->reason);
        ESP_ERROR_CHECK(context_set_network_error(args.context, true));
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_connect(void)
{
    ESP_LOGI(TAG, "Connecting to %s...", args.ssid);
    ESP_ERROR_CHECK(context_set_network_connected(args.context, false));
    ESP_ERROR_CHECK(context_set_network_error(args.context, false));

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdTRUE, pdTRUE, CONNECTION_TIMEOUT_TICKS);
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_ERROR_CHECK(context_set_network_connected(args.context, true));
        ESP_ERROR_CHECK(context_set_network_error(args.context, false));
    } else {
        ESP_LOGW(TAG, "Failed to connect to %s", args.ssid);
        ESP_ERROR_CHECK(context_set_network_connected(args.context, false));
        ESP_ERROR_CHECK(context_set_network_error(args.context, true));
    }
}

static bool wifi_is_provisioned(void)
{
    wifi_config_t wifi_config;
    if (args.ssid != NULL && args.password != NULL) {
        ESP_LOGI(TAG, "Saved ssid: %s", args.ssid);
        ESP_LOGI(TAG, "Saved password: %s", args.password);
        strlcpy((char *)wifi_config.sta.ssid, args.ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, args.password, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        return true;
    }
    return false;
}

static void wifi_start_sta(void)
{
    ESP_LOGI(TAG, "Initializing wifi...");
    esp_log_level_set("wifi", ESP_LOG_VERBOSE);
    ESP_ERROR_CHECK(context_set_network_connected(args.context, false));

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
    context_t *context = (context_t *)arg;
    wifi_start_sta();
    if (!wifi_is_provisioned()) {
        smartconfig_init(context);
    }
    while (true) {
        xEventGroupWaitBits(context->event_group, CONTEXT_EVENT_WIFI_CONFIG, pdTRUE, pdTRUE, portMAX_DELAY);
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

esp_err_t wifi_init(context_t *context, const char *ssid, const char *password)
{
    args.context = context;
    args.ssid = ssid;
    args.password = password;

    xTaskCreatePinnedToCore(wifi_task, "wifi", 4096, context, 2, NULL, tskNO_AFFINITY);
    return ESP_OK;
}