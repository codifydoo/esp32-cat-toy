#include "carousel.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_step_wraps_both_ways(void)
{
    carousel_t c;
    carousel_init(&c, 6);
    TEST_ASSERT_EQUAL_UINT8(5, carousel_step(&c, -1));
    TEST_ASSERT_EQUAL_UINT8(0, carousel_step(&c, +1));
    TEST_ASSERT_EQUAL_UINT8(1, carousel_step(&c, +1));
}

static void test_single_app_stays_put(void)
{
    carousel_t c;
    carousel_init(&c, 1);
    TEST_ASSERT_EQUAL_UINT8(0, carousel_step(&c, +1));
    TEST_ASSERT_EQUAL_UINT8(0, carousel_step(&c, -1));
}

static void test_zero_count_is_one(void)
{
    carousel_t c;
    carousel_init(&c, 0);
    TEST_ASSERT_EQUAL_UINT8(1, c.count);
}

static void test_six_apps_show_all_dots(void)
{
    carousel_t c = {.count = 6, .index = 1};
    dot_window_t w = carousel_dot_window(&c);
    TEST_ASSERT_EQUAL_UINT8(0, w.first);
    TEST_ASSERT_EQUAL_UINT8(6, w.n);
    TEST_ASSERT_EQUAL(DOT_ACTIVE, carousel_dot_kind(&c, 1));
    TEST_ASSERT_EQUAL(DOT_IDLE, carousel_dot_kind(&c, 0));
    TEST_ASSERT_EQUAL(DOT_IDLE, carousel_dot_kind(&c, 5));
}

static void test_many_apps_window_at_start(void)
{
    carousel_t c = {.count = 24, .index = 0};
    dot_window_t w = carousel_dot_window(&c);
    TEST_ASSERT_EQUAL_UINT8(0, w.first);
    TEST_ASSERT_EQUAL_UINT8(7, w.n);
    TEST_ASSERT_EQUAL(DOT_ACTIVE, carousel_dot_kind(&c, 0));
    TEST_ASSERT_EQUAL(DOT_EDGE, carousel_dot_kind(&c, 6));
    TEST_ASSERT_EQUAL(DOT_HIDDEN, carousel_dot_kind(&c, 7));
}

static void test_many_apps_window_in_middle(void)
{
    carousel_t c = {.count = 24, .index = 10};
    dot_window_t w = carousel_dot_window(&c);
    TEST_ASSERT_EQUAL_UINT8(7, w.first);
    TEST_ASSERT_EQUAL(DOT_EDGE, carousel_dot_kind(&c, 7));
    TEST_ASSERT_EQUAL(DOT_EDGE, carousel_dot_kind(&c, 13));
    TEST_ASSERT_EQUAL(DOT_ACTIVE, carousel_dot_kind(&c, 10));
    TEST_ASSERT_EQUAL(DOT_IDLE, carousel_dot_kind(&c, 9));
}

static void test_many_apps_window_at_end(void)
{
    carousel_t c = {.count = 24, .index = 23};
    dot_window_t w = carousel_dot_window(&c);
    TEST_ASSERT_EQUAL_UINT8(17, w.first);
    TEST_ASSERT_EQUAL(DOT_EDGE, carousel_dot_kind(&c, 17));
    TEST_ASSERT_EQUAL(DOT_ACTIVE, carousel_dot_kind(&c, 23));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_step_wraps_both_ways);
    RUN_TEST(test_single_app_stays_put);
    RUN_TEST(test_zero_count_is_one);
    RUN_TEST(test_six_apps_show_all_dots);
    RUN_TEST(test_many_apps_window_at_start);
    RUN_TEST(test_many_apps_window_in_middle);
    RUN_TEST(test_many_apps_window_at_end);
    return UNITY_END();
}
