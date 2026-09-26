#include "batlog.h"
#include <stdio.h>

static const char *const NAMES[BATLOG_EVENT_COUNT] = {
    [BATLOG_COLD] = "cold",     [BATLOG_WAKE_PIN] = "wake_pin", [BATLOG_WAKE_TIMER] = "wake_timer",
    [BATLOG_AWAKE] = "awake",   [BATLOG_SLEEP] = "sleep",       [BATLOG_USB_IN] = "usb_in",
    [BATLOG_USB_OUT] = "usb_out",
};

void batlog_clear(batlog_t *l)
{
    l->next = 0;
    l->count = 0;
}

void batlog_push(batlog_t *l, batlog_rec_t r)
{
    l->rec[l->next] = r;
    l->next = (uint16_t)((l->next + 1) % BATLOG_CAP);
    if (l->count < BATLOG_CAP) {
        l->count++;
    }
}

bool batlog_get(const batlog_t *l, size_t i, batlog_rec_t *out)
{
    if (i >= l->count) {
        return false;
    }
    const size_t oldest = (l->next + BATLOG_CAP - l->count) % BATLOG_CAP;
    *out = l->rec[(oldest + i) % BATLOG_CAP];
    return true;
}

bool batlog_valid(const batlog_t *l)
{
    return l->next < BATLOG_CAP && l->count <= BATLOG_CAP && (l->count == BATLOG_CAP || l->next == l->count);
}

const char *batlog_event_name(uint8_t event) { return event < BATLOG_EVENT_COUNT ? NAMES[event] : "?"; }

int batlog_format(const batlog_rec_t *r, char *buf, size_t n)
{
    const char *name = batlog_event_name(r->event);
    if (r->pct == BATLOG_NO_PCT) {
        return snprintf(buf, n, "BATLOG,%lu,-,%u,%s", (unsigned long)r->time, (unsigned)r->mv, name);
    }
    return snprintf(buf, n, "BATLOG,%lu,%u,%u,%s", (unsigned long)r->time, (unsigned)r->pct, (unsigned)r->mv, name);
}
