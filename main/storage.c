#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "context.h"
#include "storage.h"

static const char *TAG = "storage";

static nvs_handle_t handle;

esp_err_t storage_init(context_t *context)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &handle));
    return ESP_OK;
}

esp_err_t storage_get_string(const char *key, char **buf, size_t *length)
{
    size_t len = 0;
    esp_err_t err = nvs_get_str(handle, key, NULL, &len);
    switch (err) {
    case ESP_OK:
        *buf = malloc(len);
        if (*buf == NULL) {
            return ESP_ERR_NO_MEM;
        }
        if (length != NULL) {
            *length = len;
        }
        return nvs_get_str(handle, key, *buf, &len);
    case ESP_ERR_NVS_NOT_FOUND:
        return ESP_OK;
    default:
        return err;
    }
}

esp_err_t storage_set_string(const char *key, const char *buf)
{
    esp_err_t err = nvs_set_str(handle, key, buf);
    return err == ESP_OK ? nvs_commit(handle) : err;
}

esp_err_t storage_delete(const char *key)
{
    esp_err_t err = nvs_erase_key(handle, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    return err == ESP_OK ? nvs_commit(handle) : err;
}
