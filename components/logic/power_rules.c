#include "power_rules.h"

static bool valid(int pct) { return pct >= 0 && pct <= 100; }

void battery_rules_init(battery_rules_t *r)
{
    r->armed_low = true;
    r->armed_lower = true;
    r->pending = false;
}

battery_verdict_t battery_rules_update(battery_rules_t *r, int pct, bool usb)
{
    if (!valid(pct)) {
        return BATTERY_OK;
    }
    if (usb) {
        r->armed_low = true;
        r->armed_lower = true;
        r->pending = false;
        return BATTERY_OK;
    }
    if (pct >= POWER_REARM_PCT) {
        battery_rules_init(r); /* charged while asleep: USB power could not wake the chip to say so */
    }
    if (pct <= POWER_LOWER_PCT && r->armed_lower) {
        r->armed_lower = false;
        r->armed_low = false; /* a jump past both levels shows one screen */
        r->pending = true;
    } else if (pct <= POWER_LOW_PCT && r->armed_low) {
        r->armed_low = false;
        r->pending = true;
    }
    return battery_critical(pct, usb) ? BATTERY_CRITICAL : BATTERY_OK;
}

bool battery_rules_take_pending(battery_rules_t *r)
{
    const bool p = r->pending;
    r->pending = false;
    return p;
}

bool battery_critical(int pct, bool usb) { return !usb && valid(pct) && pct <= POWER_CRITICAL_PCT; }

wake_plan_t power_wake_plan(woke_t woke, bool night, bool battery_sleep, bool critical)
{
    if (woke != WOKE_COLD && battery_sleep && critical) {
        return WAKE_HUNGRY;
    }
    switch (woke) {
    case WOKE_TIMER: return night ? WAKE_LOCK_DARK : WAKE_SUNRISE;
    case WOKE_TOUCH: return night ? WAKE_LOCK_LIT : WAKE_BOOT;
    default: return WAKE_BOOT;
    }
}
