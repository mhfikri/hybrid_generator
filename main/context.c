#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_err.h"

#include "context.h"

#define context_set(p, v, f) do {                    \
      if ((p) != (v)) {                              \
        (p) = (v);                                   \
        bitsToSet |= (f);                            \
      }                                              \
    } while (0)

#define context_set_flags(c, v, f) do {              \
      if (v) {                                       \
        xEventGroupSetBits((c)->event_group, (f));   \
      } else {                                       \
        xEventGroupClearBits((c)->event_group, (f)); \
      }                                              \
    } while (0)

static const char *TAG = "config";

context_t *context_create(void)
{
    context_t *context = calloc(1, sizeof(context_t));

    context->event_group = xEventGroupCreate();

    return context;
}

esp_err_t context_set_wifi_config(context_t *context, const char *ssid, const char *password)
{
    EventBits_t bitsToSet = 0U;
    context_set(context->config.ssid, ssid, CONTEXT_EVENT_WIFI_CONFIG);
    context_set(context->config.password, password, CONTEXT_EVENT_WIFI_CONFIG);

    if (bitsToSet) xEventGroupSetBits(context->event_group, bitsToSet);
    return ESP_OK;
}

esp_err_t context_set_network_connected(context_t *context, bool connected)
{
    context_set_flags(context, connected, CONTEXT_EVENT_NETWORK);
    return ESP_OK;
}

esp_err_t context_set_network_error(context_t *context, bool error)
{
    context_set_flags(context, error, CONTEXT_EVENT_NETWORK_ERROR);
    return ESP_OK;
}

esp_err_t context_set_time_updated(context_t *context)
{
    xEventGroupSetBits(context->event_group, CONTEXT_EVENT_TIME);
    return ESP_OK;
}
