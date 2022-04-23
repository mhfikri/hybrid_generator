#ifndef HYBRID_GENERATOR_CONTEXT_H
#define HYBRID_GENERATOR_CONTEXT_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_bit_defs.h"

typedef enum {
    CONTEXT_EVENT_WIFI_CONFIG = BIT0,
    CONTEXT_EVENT_NETWORK = BIT1,
    CONTEXT_EVENT_NETWORK_ERROR = BIT2,
    CONTEXT_EVENT_TIME = BIT3,
} context_event_t;

typedef struct {
    EventGroupHandle_t event_group;
    struct {
        const char *device_id;
        const char *ssid;
        const char *password;
        const char *firmware_version;
    } config;
} context_t;

context_t *context_create(void);

esp_err_t context_set_network_connected(context_t *context, bool connected);

esp_err_t context_set_network_error(context_t *context, bool error);

esp_err_t context_set_time_updated(context_t *context);

esp_err_t context_set_wifi_config(context_t *context, const char *ssid, const char *password);

#endif /* HYBRID_GENERATOR_CONTEXT_H */
