#pragma once
#include <stdbool.h>

/* Power state (spec §3.12). M7 adds the battery level, charging and deep sleep. */
void power_init(void);    /* call once, after display_init() (the I2C bus is up) */
bool power_on_usb(void);  /* the AXP2101 "VBUS good" bit; false when unknown. One I2C read. */
