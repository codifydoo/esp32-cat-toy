#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The battery log of the batlog dev build (spec §3.14): the last BATLOG_CAP readings, oldest first. Pure: no
 * ESP-IDF. The platform keeps one batlog_t in NVS. */
#define BATLOG_CAP           256
#define BATLOG_NO_PCT        0xFF /* no gauge reading */
#define BATLOG_AWAKE_EVERY_S 600  /* a reading every 10 minutes while awake */
#define BATLOG_LINE_MAX      48   /* batlog_format() output, with its terminator */

typedef enum {
    BATLOG_COLD,       /* a start from power-on or a reset */
    BATLOG_WAKE_PIN,   /* a start from deep sleep by a touch or BOOT */
    BATLOG_WAKE_TIMER, /* a start from deep sleep by the timer */
    BATLOG_AWAKE,      /* every BATLOG_AWAKE_EVERY_S while awake */
    BATLOG_SLEEP,      /* just before a deep sleep */
    BATLOG_USB_IN,
    BATLOG_USB_OUT,
    BATLOG_EVENT_COUNT,
} batlog_event_t;

typedef struct {
    uint32_t time; /* the clock time in Unix seconds, as the toy knows it (1970-based until the clock is set) */
    uint16_t mv;   /* the battery voltage */
    uint8_t pct;   /* the gauge, or BATLOG_NO_PCT */
    uint8_t event; /* a batlog_event_t */
} batlog_rec_t;

typedef struct {
    batlog_rec_t rec[BATLOG_CAP];
    uint16_t next;  /* where the next record goes */
    uint16_t count; /* records held, at most BATLOG_CAP */
} batlog_t;

void batlog_clear(batlog_t *l);
/* Adds a record; once full, it replaces the oldest. */
void batlog_push(batlog_t *l, batlog_rec_t r);
/* The i-th oldest record (0 is the oldest); false past the last one. */
bool batlog_get(const batlog_t *l, size_t i, batlog_rec_t *out);
/* The indexes are in range: a log loaded from flash that fails this is started afresh. */
bool batlog_valid(const batlog_t *l);
/* "BATLOG,<unix>,<pct or ->,<mV>,<event name>" into buf (BATLOG_LINE_MAX is enough); returns its length. */
int batlog_format(const batlog_rec_t *r, char *buf, size_t n);
/* "cold", "wake_pin", ...; "?" for anything else. */
const char *batlog_event_name(uint8_t event);
