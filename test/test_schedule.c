#include "schedule.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define HM(h, m) ((uint16_t)((h) * 60 + (m)))

static void test_the_default_night_runs_across_midnight(void)
{
    const uint16_t bed = HM(19, 30), wake = HM(7, 0);
    TEST_ASSERT_FALSE(schedule_is_night(HM(19, 29), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(19, 30), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(23, 59), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(0, 0), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(6, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(7, 0), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(12, 0), bed, wake));
}

static void test_a_night_that_starts_after_midnight(void)
{
    const uint16_t bed = HM(0, 30), wake = HM(6, 0);
    TEST_ASSERT_FALSE(schedule_is_night(HM(23, 0), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(0, 29), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(0, 30), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(5, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(6, 0), bed, wake));
}

static void test_equal_times_mean_no_night(void)
{
    for (uint16_t now = 0; now < SCHEDULE_DAY_MIN; now += 15) {
        TEST_ASSERT_FALSE(schedule_is_night(now, HM(7, 0), HM(7, 0)));
    }
}

/* DST (spec §7): the lock follows the clock's state, not the arrival of an exact minute. On the spring
 * change the clock jumps from 01:59 to 03:00, so a 02:30 wake-up minute never shows, and the night must
 * still end. On the autumn change 02:00-02:59 comes twice, so that hour is night again. */
static void test_dst_jumps_still_end_the_night(void)
{
    const uint16_t bed = HM(19, 30), wake = HM(2, 30);
    TEST_ASSERT_TRUE(schedule_is_night(HM(1, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(3, 0), bed, wake)); /* spring: 01:59 -> 03:00 */
    TEST_ASSERT_TRUE(schedule_is_night(HM(2, 0), bed, wake));  /* autumn: 02:59 -> 02:00 again */
}

static void test_minutes_until_wraps_forward(void)
{
    TEST_ASSERT_EQUAL_UINT16(690, schedule_minutes_until(HM(19, 30), HM(7, 0)));
    TEST_ASSERT_EQUAL_UINT16(1, schedule_minutes_until(HM(6, 59), HM(7, 0)));
    TEST_ASSERT_EQUAL_UINT16(0, schedule_minutes_until(HM(7, 0), HM(7, 0)));
    TEST_ASSERT_EQUAL_UINT16(1439, schedule_minutes_until(HM(7, 1), HM(7, 0)));
}

static void test_hour_stars_round_up_and_stop_at_five(void)
{
    const uint16_t wake = HM(7, 0);
    TEST_ASSERT_EQUAL_UINT8(5, schedule_stars_left(HM(19, 30), wake)); /* 11 h 30 left */
    TEST_ASSERT_EQUAL_UINT8(5, schedule_stars_left(HM(2, 0), wake));   /* 5 h */
    TEST_ASSERT_EQUAL_UINT8(5, schedule_stars_left(HM(2, 1), wake));   /* 4 h 59 */
    TEST_ASSERT_EQUAL_UINT8(4, schedule_stars_left(HM(3, 0), wake));   /* 4 h */
    TEST_ASSERT_EQUAL_UINT8(1, schedule_stars_left(HM(6, 0), wake));   /* 1 h */
    TEST_ASSERT_EQUAL_UINT8(1, schedule_stars_left(HM(6, 59), wake));  /* 1 min */
    TEST_ASSERT_EQUAL_UINT8(0, schedule_stars_left(HM(7, 0), wake));   /* wake time */
}

static void test_steps_wrap_around_the_day(void)
{
    TEST_ASSERT_EQUAL_UINT16(HM(23, 45), schedule_step(HM(0, 0), -15));
    TEST_ASSERT_EQUAL_UINT16(HM(0, 0), schedule_step(HM(23, 45), 15));
    TEST_ASSERT_EQUAL_UINT16(HM(12, 1), schedule_step(HM(12, 0), 1));
    TEST_ASSERT_EQUAL_UINT16(HM(23, 55), schedule_step(HM(0, 5), -10));
    TEST_ASSERT_EQUAL_UINT16(HM(19, 30), schedule_step(HM(19, 30), 1440));
}

static void test_format_is_two_digit_hours_and_minutes(void)
{
    char s[6];
    schedule_format(0, s);
    TEST_ASSERT_EQUAL_STRING("00:00", s);
    schedule_format(HM(7, 5), s);
    TEST_ASSERT_EQUAL_STRING("07:05", s);
    schedule_format(HM(19, 30), s);
    TEST_ASSERT_EQUAL_STRING("19:30", s);
    schedule_format(HM(23, 59), s);
    TEST_ASSERT_EQUAL_STRING("23:59", s);
}

static void test_now_speeds_up_after_two_seconds(void)
{
    TEST_ASSERT_EQUAL_UINT16(1, schedule_now_step(0));
    TEST_ASSERT_EQUAL_UINT16(1, schedule_now_step(1999));
    TEST_ASSERT_EQUAL_UINT16(10, schedule_now_step(2000));
    TEST_ASSERT_EQUAL_UINT16(10, schedule_now_step(60000));
}

/* While the clock is not valid, a sync retries every 5 minutes regardless of now or wake. */
static void test_sync_due_retries_every_5_min_while_invalid(void)
{
    TEST_ASSERT_FALSE(schedule_sync_due(false, HM(3, 17), HM(7, 0), 299));
    TEST_ASSERT_TRUE(schedule_sync_due(false, HM(3, 17), HM(7, 0), 300));
    TEST_ASSERT_TRUE(schedule_sync_due(false, HM(3, 17), HM(7, 0), UINT32_MAX));
}

/* Once valid, the daily sync is due 10 minutes before wake, and only an hour after the last attempt. */
static void test_sync_due_ten_minutes_before_wake(void)
{
    const uint16_t wake = HM(7, 0);
    TEST_ASSERT_TRUE(schedule_sync_due(true, HM(6, 50), wake, 3600));
    TEST_ASSERT_FALSE(schedule_sync_due(true, HM(6, 50), wake, 3599));
    TEST_ASSERT_TRUE(schedule_sync_due(true, HM(6, 50), wake, UINT32_MAX));
}

static void test_sync_not_due_at_other_minutes(void)
{
    const uint16_t wake = HM(7, 0);
    TEST_ASSERT_FALSE(schedule_sync_due(true, wake, wake, UINT32_MAX));       /* the wake minute itself */
    TEST_ASSERT_FALSE(schedule_sync_due(true, HM(6, 51), wake, UINT32_MAX)); /* wake - 9 */
    TEST_ASSERT_FALSE(schedule_sync_due(true, HM(6, 49), wake, UINT32_MAX)); /* wake - 11 */
}

static void test_sync_due_wraps_past_midnight(void)
{
    const uint16_t wake = HM(0, 5);
    TEST_ASSERT_TRUE(schedule_sync_due(true, HM(23, 55), wake, UINT32_MAX));
    TEST_ASSERT_FALSE(schedule_sync_due(true, wake, wake, UINT32_MAX));
}

static void test_sleep_allowed_except_in_the_last_quarter_hour_before_wake(void)
{
    const uint16_t bed = HM(19, 30), wake = HM(7, 0);
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(15, 0), bed, wake));
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(22, 0), bed, wake));
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(6, 44), bed, wake)); /* 16 minutes before */
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(6, 45), bed, wake)); /* 15 minutes before */
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(6, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(7, 0), bed, wake)); /* the wake minute: the sunrise */
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(7, 1), bed, wake));
}

static void test_sleep_allowed_any_time_without_a_night_or_a_valid_clock(void)
{
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(6, 55), HM(7, 0), HM(7, 0))); /* bed == wake */
    TEST_ASSERT_TRUE(schedule_sleep_allowed(false, HM(6, 55), HM(19, 30), HM(7, 0)));
}

static void test_sleep_guard_wraps_past_midnight(void)
{
    const uint16_t bed = HM(22, 0), wake = HM(0, 5);
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(23, 55), bed, wake));
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(23, 49), bed, wake));
}

static void test_wake_timer_minute_is_ten_minutes_before_wake(void)
{
    TEST_ASSERT_EQUAL_INT32(HM(6, 50), schedule_wake_timer_min(true, HM(19, 30), HM(7, 0)));
    TEST_ASSERT_EQUAL_INT32(HM(23, 55), schedule_wake_timer_min(true, HM(22, 0), HM(0, 5)));
}

static void test_no_wake_timer_without_a_night_or_a_valid_clock(void)
{
    TEST_ASSERT_EQUAL_INT32(-1, schedule_wake_timer_min(false, HM(19, 30), HM(7, 0)));
    TEST_ASSERT_EQUAL_INT32(-1, schedule_wake_timer_min(true, HM(7, 0), HM(7, 0)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_default_night_runs_across_midnight);
    RUN_TEST(test_a_night_that_starts_after_midnight);
    RUN_TEST(test_equal_times_mean_no_night);
    RUN_TEST(test_dst_jumps_still_end_the_night);
    RUN_TEST(test_minutes_until_wraps_forward);
    RUN_TEST(test_hour_stars_round_up_and_stop_at_five);
    RUN_TEST(test_steps_wrap_around_the_day);
    RUN_TEST(test_format_is_two_digit_hours_and_minutes);
    RUN_TEST(test_now_speeds_up_after_two_seconds);
    RUN_TEST(test_sync_due_retries_every_5_min_while_invalid);
    RUN_TEST(test_sync_due_ten_minutes_before_wake);
    RUN_TEST(test_sync_not_due_at_other_minutes);
    RUN_TEST(test_sync_due_wraps_past_midnight);
    RUN_TEST(test_sleep_allowed_except_in_the_last_quarter_hour_before_wake);
    RUN_TEST(test_sleep_allowed_any_time_without_a_night_or_a_valid_clock);
    RUN_TEST(test_sleep_guard_wraps_past_midnight);
    RUN_TEST(test_wake_timer_minute_is_ten_minutes_before_wake);
    RUN_TEST(test_no_wake_timer_without_a_night_or_a_valid_clock);
    return UNITY_END();
}
