#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_smartconfig.h"
#include "esp_wifi.h"

#include "context.h"
#include "smartconfig.h"
#include "storage.h"

#define SC_DONE_BIT BIT0

static const char *TAG = "smartconfig";

static EventGroupHandle_t sc_event_group;

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event base: %s, id: %d", event_base, event_id);
    if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        context_t *context = (context_t *)arg;
        smartconfig_event_got_ssid_pswd_t *event = (smartconfig_event_got_ssid_pswd_t *)event_data;

        wifi_config_t wifi_cfg;
        bzero(&wifi_cfg, sizeof(wifi_config_t));
        memcpy(wifi_cfg.sta.ssid, event->ssid, sizeof(wifi_cfg.sta.ssid));
        memcpy(wifi_cfg.sta.password, event->password, sizeof(wifi_cfg.sta.password));

        memcpy(context->config.ssid, event->ssid, sizeof(event->ssid));
        memcpy(context->config.password, event->password, sizeof(event->password));
        ESP_LOGI(TAG, "Found ssid: %s", (const char *)wifi_cfg.sta.ssid);
        ESP_LOGI(TAG, "Found password: %s", (const char *)wifi_cfg.sta.password);

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_ERROR_CHECK(context_set_network_provisioned(context));
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(sc_event_group, SC_DONE_BIT);
    }
}

static void smartconfig_task(void *arg)
{
    context_t *context = (context_t *)arg;

    ESP_LOGI(TAG, "Starting smartconfig...");
    esp_log_level_set("smartconfig", ESP_LOG_VERBOSE);

    sc_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, context));
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (true) {
        xEventGroupWaitBits(sc_event_group, SC_DONE_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Smartconfig finished");
        ESP_ERROR_CHECK(esp_smartconfig_stop());
        vTaskDelete(NULL);
    }
}
esp_err_t smartconfig_init(context_t *context)
{
    xTaskCreate(smartconfig_task, "smartconfig", 4096, context, 3, NULL);
    return ESP_OK;
}