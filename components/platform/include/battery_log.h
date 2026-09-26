#pragma once
#include <stdbool.h>
#include "sdkconfig.h"

/* The batlog dev build's battery log (spec §3.14, CONFIG_CAT_DEV_BATTERY_LOG): the last 256 readings of the
 * gauge and the battery voltage, kept in NVS and printed as CSV every 30 s while USB is connected. Without the
 * option both calls do nothing. */
#if CONFIG_CAT_DEV_BATTERY_LOG
/* After power_init() (NVS and the first gauge reading are ready): loads the log, records this start and starts
 * the logging task. */
void battery_log_init(void);
/* From deepsleep_start(), just before the chip sleeps: records the sleep, and records nothing after it. */
void battery_log_sleep(void);
/* The panel went dark (true) or lit again (false) while awake. Any task, never blocks: the logging task records
 * it within a second, so play time and the dark minutes before a sleep can be told apart. */
void battery_log_panel(bool dark);
#else
static inline void battery_log_init(void) {}
static inline void battery_log_sleep(void) {}
static inline void battery_log_panel(bool dark) { (void)dark; }
#endif
