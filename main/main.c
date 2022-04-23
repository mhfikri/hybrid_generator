#include "esp_log.h"

#include "context.h"
#include "storage.h"
#include "wifi.h"

static context_t *context;

void app_main(void)
{
    context = context_create();
    ESP_ERROR_CHECK(storage_init(context));
    ESP_ERROR_CHECK(wifi_init(context, context->config.ssid, context->config.password));
}
