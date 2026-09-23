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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_small_movement_is_a_tap);
    RUN_TEST(test_tap_limit_is_20px);
    RUN_TEST(test_wandering_finger_is_not_a_tap);
    RUN_TEST(test_swipes);
    RUN_TEST(test_swipe_limits);
    RUN_TEST(test_end_without_begin);
    return UNITY_END();
}
