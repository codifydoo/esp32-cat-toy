#include "schedule.h"

bool schedule_is_night(uint16_t now, uint16_t bed, uint16_t wake)
{
    if (bed == wake) {
        return false;
    }
    if (bed < wake) {
        return now >= bed && now < wake; /* a night that starts after midnight */
    }
    return now >= bed || now < wake; /* across midnight */
}

uint16_t schedule_minutes_until(uint16_t now, uint16_t target)
{
    return (uint16_t)(((uint32_t)target + SCHEDULE_DAY_MIN - now) % SCHEDULE_DAY_MIN);
}

uint8_t schedule_stars_left(uint16_t now, uint16_t wake)
{
    const uint32_t hours = ((uint32_t)schedule_minutes_until(now, wake) + 59) / 60;
    return (uint8_t)(hours > SCHEDULE_STARS ? SCHEDULE_STARS : hours);
}

uint16_t schedule_step(uint16_t min, int32_t delta)
{
    const int32_t v = ((int32_t)min + delta) % SCHEDULE_DAY_MIN;
    return (uint16_t)(v < 0 ? v + SCHEDULE_DAY_MIN : v);
}

void schedule_format(uint16_t min, char out[6])
{
    const unsigned m = min % SCHEDULE_DAY_MIN;
    const unsigned h = m / 60, mm = m % 60;
    out[0] = (char)('0' + h / 10);
    out[1] = (char)('0' + h % 10);
    out[2] = ':';
    out[3] = (char)('0' + mm / 10);
    out[4] = (char)('0' + mm % 10);
    out[5] = '\0';
}

uint16_t schedule_now_step(uint32_t held_ms)
{
    return held_ms >= SCHEDULE_NOW_FAST_MS ? SCHEDULE_NOW_FAST_STEP : 1;
}

bool schedule_sync_due(bool valid, uint16_t now, uint16_t wake, uint32_t secs_since_attempt)
{
    if (!valid) {
        return secs_since_attempt >= SCHEDULE_SYNC_RETRY_S;
    }
    return now == schedule_step(wake, -SCHEDULE_SYNC_BEFORE_MIN) && secs_since_attempt >= SCHEDULE_SYNC_GAP_S;
}

bool schedule_sleep_allowed(bool valid, uint16_t now, uint16_t bed, uint16_t wake)
{
    if (!valid || bed == wake) {
        return true;
    }
    return schedule_minutes_until(now, wake) > SCHEDULE_SLEEP_GUARD_MIN;
}

int32_t schedule_wake_timer_min(bool valid, uint16_t bed, uint16_t wake)
{
    if (!valid || bed == wake) {
        return -1;
    }
    return schedule_step(wake, -SCHEDULE_WAKE_LEAD_MIN);
}
