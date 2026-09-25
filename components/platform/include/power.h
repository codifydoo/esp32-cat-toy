#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Power state (spec §3.12, §3.13): USB power, the battery gauge and the charger. */
typedef struct {
    int8_t pct;    /* battery 0-100 %, or -1 when the gauge is unavailable (spec §4.7) */
    bool usb;      /* USB power: the AXP2101 "VBUS good" bit */
    bool charging; /* the charger reports current into the battery */
} power_state_t;

/* Call once, after display_init() (the I2C bus is up): turns the fuel gauge on, takes a first reading. */
void power_init(void);
/* The "VBUS good" bit read now (one I2C read, any task); false when unknown. */
bool power_on_usb(void);
/* Reads USB and the charger, and the gauge when with_gauge. The sensors task calls it every 500 ms, with the
 * gauge every 5 s (spec §4.5). The result is kept for power_state(). */
void power_poll(bool with_gauge);
/* The latest power_poll() result; any task. */
power_state_t power_state(void);
