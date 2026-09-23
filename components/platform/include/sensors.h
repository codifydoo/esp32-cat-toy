#pragma once
#include "buttons.h"
#include "esp_err.h"

typedef struct {
    void (*on_button)(button_id_t id); /* runs on the sensor task */
} sensors_listener_t;

esp_err_t sensors_start(const sensors_listener_t *listener);
