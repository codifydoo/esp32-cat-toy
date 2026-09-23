#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef enum { BUTTON_HOME, BUTTON_POWER } button_id_t;

#define BUTTON_EV_HOME  (1u << 0)
#define BUTTON_EV_POWER (1u << 1)

esp_err_t buttons_init(void);
/* Call every 20 ms. Returns BUTTON_EV_* bits for debounced presses since the last call. */
uint8_t buttons_poll(void);
