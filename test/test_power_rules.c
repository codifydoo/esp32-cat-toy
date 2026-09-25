#include "power_rules.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static battery_rules_t fresh(void)
{
    battery_rules_t r;
    battery_rules_init(&r);
    return r;
}

static void test_no_screen_above_ten_percent(void)
{
    battery_rules_t r = fresh();
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 50, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 11, false));
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_ten_percent_shows_once(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 10, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r)); /* taken */
    battery_rules_update(&r, 9, false);
    battery_rules_update(&r, 10, false); /* gauge jitter back up and down */
    battery_rules_update(&r, 8, false);
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_five_percent_shows_once_more(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 10, false);
    battery_rules_take_pending(&r);
    battery_rules_update(&r, 5, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 4, false);
    battery_rules_update(&r, 3, false);
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_a_jump_past_both_levels_shows_one_screen(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 50, false);
    battery_rules_update(&r, 4, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 4, false);
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_a_pending_screen_waits_until_taken(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 9, false);
    battery_rules_update(&r, 9, false);
    battery_rules_update(&r, 8, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
}

static void test_usb_power_arms_both_levels_again_and_drops_a_pending_screen(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 4, false); /* pending, both levels used */
    battery_rules_update(&r, 4, true);  /* plugged in: charging, no hungry cat */
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 12, false); /* unplugged */
    battery_rules_update(&r, 10, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 5, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
}

static void test_critical_only_on_battery_at_two_percent_or_less(void)
{
    battery_rules_t r = fresh();
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 3, false));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, battery_rules_update(&r, 2, false));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, battery_rules_update(&r, 0, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 0, true));
    TEST_ASSERT_TRUE(battery_critical(2, false));
    TEST_ASSERT_FALSE(battery_critical(3, false));
    TEST_ASSERT_FALSE(battery_critical(1, true));
}

static void test_an_unavailable_or_wild_gauge_never_shows_or_sleeps(void)
{
    battery_rules_t r = fresh();
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, -1, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 101, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 255, false));
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
    TEST_ASSERT_FALSE(battery_critical(-1, false));
    TEST_ASSERT_FALSE(battery_critical(200, false));
    battery_rules_update(&r, 9, false); /* the gauge comes back: the levels are still armed */
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
}

static void test_wake_plan_after_a_cold_start_is_the_boot_animation(void)
{
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_COLD, false, false, false));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_COLD, true, false, false)); /* then the lock, as today */
}

static void test_wake_plan_after_a_touch(void)
{
    TEST_ASSERT_EQUAL(WAKE_LOCK_LIT, power_wake_plan(WOKE_TOUCH, true, false, false));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_TOUCH, false, false, false));
}

static void test_wake_plan_after_the_timer(void)
{
    TEST_ASSERT_EQUAL(WAKE_LOCK_DARK, power_wake_plan(WOKE_TIMER, true, false, false));
    TEST_ASSERT_EQUAL(WAKE_SUNRISE, power_wake_plan(WOKE_TIMER, false, false, false)); /* woke after wake time */
}

static void test_wake_plan_while_the_battery_is_still_empty(void)
{
    TEST_ASSERT_EQUAL(WAKE_HUNGRY, power_wake_plan(WOKE_TOUCH, false, true, true));
    TEST_ASSERT_EQUAL(WAKE_HUNGRY, power_wake_plan(WOKE_TOUCH, true, true, true));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_TOUCH, false, true, false)); /* charged, or on USB now */
    TEST_ASSERT_EQUAL(WAKE_LOCK_LIT, power_wake_plan(WOKE_TOUCH, true, true, false));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_TOUCH, false, false, true)); /* not a battery sleep */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_screen_above_ten_percent);
    RUN_TEST(test_ten_percent_shows_once);
    RUN_TEST(test_five_percent_shows_once_more);
    RUN_TEST(test_a_jump_past_both_levels_shows_one_screen);
    RUN_TEST(test_a_pending_screen_waits_until_taken);
    RUN_TEST(test_usb_power_arms_both_levels_again_and_drops_a_pending_screen);
    RUN_TEST(test_critical_only_on_battery_at_two_percent_or_less);
    RUN_TEST(test_an_unavailable_or_wild_gauge_never_shows_or_sleeps);
    RUN_TEST(test_wake_plan_after_a_cold_start_is_the_boot_animation);
    RUN_TEST(test_wake_plan_after_a_touch);
    RUN_TEST(test_wake_plan_after_the_timer);
    RUN_TEST(test_wake_plan_while_the_battery_is_still_empty);
    return UNITY_END();
}
