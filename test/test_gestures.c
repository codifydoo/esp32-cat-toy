#include "gestures.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static gesture_kind_t run(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint32_t ms)
{
    gesture_track_t g = {0};
    gesture_begin(&g, x0, y0, 0);
    return gesture_end(&g, x1, y1, ms);
}

static void test_small_movement_is_a_tap(void)
{
    gesture_track_t g = {0};
    gesture_begin(&g, 100, 100, 0);
    gesture_move(&g, 105, 103);
    TEST_ASSERT_EQUAL(GESTURE_TAP, gesture_end(&g, 104, 102, 120));
}

static void test_tap_limit_is_20px(void)
{
    TEST_ASSERT_EQUAL(GESTURE_TAP, run(0, 0, 20, 0, 50));
    TEST_ASSERT_EQUAL(GESTURE_NONE, run(0, 0, 21, 0, 50));
}

static void test_wandering_finger_is_not_a_tap(void)
{
    gesture_track_t g = {0};
    gesture_begin(&g, 100, 100, 0);
    gesture_move(&g, 130, 100);
    TEST_ASSERT_EQUAL(GESTURE_NONE, gesture_end(&g, 101, 100, 100));
}

static void test_swipes(void)
{
    TEST_ASSERT_EQUAL(GESTURE_SWIPE_LEFT, run(300, 200, 200, 210, 200));
    TEST_ASSERT_EQUAL(GESTURE_SWIPE_RIGHT, run(100, 200, 170, 200, 150));
}

static void test_swipe_limits(void)
{
    TEST_ASSERT_EQUAL(GESTURE_SWIPE_LEFT, run(300, 200, 240, 200, 100)); /* exactly 60 px */
    TEST_ASSERT_EQUAL(GESTURE_NONE, run(300, 200, 241, 200, 100));       /* 59 px */
    TEST_ASSERT_EQUAL(GESTURE_SWIPE_LEFT, run(300, 200, 200, 200, 400)); /* exactly 400 ms */
    TEST_ASSERT_EQUAL(GESTURE_NONE, run(300, 200, 200, 200, 401));       /* too slow */
    TEST_ASSERT_EQUAL(GESTURE_SWIPE_LEFT, run(200, 300, 125, 250, 100)); /* |dx| = 1.5 |dy| */
    TEST_ASSERT_EQUAL(GESTURE_NONE, run(200, 300, 130, 250, 100));       /* too vertical */
}

static void test_end_without_begin(void)
{
    gesture_track_t g = {0};
    TEST_ASSERT_EQUAL(GESTURE_NONE, gesture_end(&g, 10, 10, 10));
}

static hold_state_t two(hold_track_t *h, int16_t ax, int16_t ay, int16_t bx, int16_t by, uint32_t t,
                        uint16_t *p)
{
    const int16_t x[2] = {ax, bx}, y[2] = {ay, by};
    return hold_update(h, 2, x, y, t, p);
}

static hold_state_t lift(hold_track_t *h, uint32_t t)
{
    const int16_t z[2] = {0, 0};
    return hold_update(h, 0, z, z, t, NULL);
}

static void test_two_steady_fingers_fire_once_at_five_seconds(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p = 0;
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 300, 200, 0, &p));
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, 4999, &p));
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, 5000, &p));
    TEST_ASSERT_EQUAL(HOLD_IDLE, two(&h, 100, 200, 300, 200, 9000, &p)); /* no second fire */
}

static void test_ring_shows_after_one_second_and_fills_over_four(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p = 77;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 300, 200, 999, &p));
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, 1000, &p));
    TEST_ASSERT_EQUAL_UINT16(0, p);
    two(&h, 100, 200, 300, 200, 3000, &p);
    TEST_ASSERT_EQUAL_UINT16(500, p);
    two(&h, 100, 200, 300, 200, 4999, &p);
    TEST_ASSERT_EQUAL_UINT16(999, p);
}

static void test_moving_forty_px_is_allowed(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 124, 232, 300, 200, 2000, &p)); /* 24-32-40 triangle */
}

static void test_moving_more_than_forty_px_cancels_until_a_finger_lifts(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_IDLE, two(&h, 100, 200, 341, 200, 2000, &p));
    TEST_ASSERT_EQUAL(HOLD_IDLE, two(&h, 100, 200, 341, 200, 8000, &p)); /* still cancelled */
    TEST_ASSERT_EQUAL(HOLD_IDLE, lift(&h, 8100));
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 341, 200, 8200, &p)); /* fresh start */
}

static void test_lifting_a_finger_restarts_from_zero(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    two(&h, 100, 200, 300, 200, 4000, &p);
    const int16_t x[2] = {100, 0}, y[2] = {200, 0};
    TEST_ASSERT_EQUAL(HOLD_IDLE, hold_update(&h, 1, x, y, 4100, &p));
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 300, 200, 4200, &p));
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, 9100, &p)); /* 4.9 s since restart */
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, 9200, &p));
}

static void test_one_finger_never_fires(void)
{
    hold_track_t h;
    hold_reset(&h);
    const int16_t x[2] = {233, 0}, y[2] = {100, 0};
    for (uint32_t t = 0; t <= 20000; t += 50) {
        TEST_ASSERT_EQUAL(HOLD_IDLE, hold_update(&h, 1, x, y, t, NULL));
    }
}

static void test_fingers_reported_in_swapped_order_are_not_a_move(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 302, 201, 101, 199, 2000, &p));
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, 5000, &p));
}

static void test_hold_survives_the_millisecond_counter_wrapping(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    const uint32_t t0 = UINT32_MAX - 1000u;
    two(&h, 100, 200, 300, 200, t0, &p);
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, t0 + 4999u, &p));
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, t0 + 5000u, &p));
}

static void test_the_hold_takes_over_the_touch_only_once_the_ring_shows(void)
{
    /* Under 1 s the touch still belongs to the menu: a quick two-finger tap or a palm on a dimmed
     * screen must still tap or wake. */
    TEST_ASSERT_FALSE(hold_owns_touch(HOLD_IDLE));
    TEST_ASSERT_FALSE(hold_owns_touch(HOLD_WAITING));
    TEST_ASSERT_TRUE(hold_owns_touch(HOLD_PROGRESS));
    TEST_ASSERT_TRUE(hold_owns_touch(HOLD_DONE));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_small_movement_is_a_tap);
    RUN_TEST(test_tap_limit_is_20px);
    RUN_TEST(test_wandering_finger_is_not_a_tap);
    RUN_TEST(test_swipes);
    RUN_TEST(test_swipe_limits);
    RUN_TEST(test_end_without_begin);
    RUN_TEST(test_two_steady_fingers_fire_once_at_five_seconds);
    RUN_TEST(test_ring_shows_after_one_second_and_fills_over_four);
    RUN_TEST(test_moving_forty_px_is_allowed);
    RUN_TEST(test_moving_more_than_forty_px_cancels_until_a_finger_lifts);
    RUN_TEST(test_lifting_a_finger_restarts_from_zero);
    RUN_TEST(test_one_finger_never_fires);
    RUN_TEST(test_fingers_reported_in_swapped_order_are_not_a_move);
    RUN_TEST(test_hold_survives_the_millisecond_counter_wrapping);
    RUN_TEST(test_the_hold_takes_over_the_touch_only_once_the_ring_shows);
    return UNITY_END();
}
