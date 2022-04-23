#ifndef HYBRID_GENERATOR_WIFI_H
#define HYBRID_GENERATOR_WIFI_H

#include "esp_err.h"

#include "context.h"

esp_err_t wifi_init(context_t *context, const char *ssid, const char *password);

#endif /* HYBRID_GENERATOR_WIFI_H */
