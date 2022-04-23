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
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        char *ssid = malloc(sizeof(evt->ssid));
        char *password = malloc(sizeof(evt->password));

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_ERROR_CHECK(storage_set_string(STORAGE_KEY_WIFI_SSID, ssid));
        ESP_ERROR_CHECK(storage_set_string(STORAGE_KEY_WIFI_PASSWORD, password));
        ESP_ERROR_CHECK(context_set_wifi_config(context, ssid, password));
        ESP_LOGI(TAG, "Found ssid: %s", ssid);
        ESP_LOGI(TAG, "Found password: %s", password);

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
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
        ESP_ERROR_CHECK(esp_smartconfig_stop());
        ESP_LOGI(TAG, "Smartconfig finished");
        vTaskDelete(NULL);
    }
}
esp_err_t smartconfig_init(context_t *context)
{
    xTaskCreate(smartconfig_task, "smartconfig", 4096, context, 3, NULL);
    return ESP_OK;
}