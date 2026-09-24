#include <math.h>
#include "motion.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- shake ---- */

static bool hit(shake_t *s, uint32_t t) { return shake_update(s, 0.0f, 0.0f, 2.5f, t); }
static bool calm(shake_t *s, uint32_t t) { return shake_update(s, 0.0f, 0.0f, 1.0f, t); }

static void test_three_strong_samples_within_300ms_are_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    TEST_ASSERT_FALSE(hit(&s, 0));
    TEST_ASSERT_FALSE(hit(&s, 20));
    TEST_ASSERT_TRUE(hit(&s, 40));
}

static void test_a_jolt_of_two_strong_samples_is_not_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    TEST_ASSERT_FALSE(hit(&s, 0));
    TEST_ASSERT_FALSE(hit(&s, 20));
    for (uint32_t t = 40; t < 2000; t += 20) {
        TEST_ASSERT_FALSE(calm(&s, t));
    }
}

static void test_samples_spread_over_more_than_300ms_are_not_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    TEST_ASSERT_FALSE(hit(&s, 0));
    TEST_ASSERT_FALSE(hit(&s, 200));
    TEST_ASSERT_FALSE(hit(&s, 301)); /* 301 ms after the first */
    TEST_ASSERT_TRUE(hit(&s, 500));  /* 200, 301 and 500 fit in 300 ms */
}

static void test_exactly_2_2g_does_not_count(void)
{
    shake_t s;
    shake_reset(&s);
    for (uint32_t t = 0; t < 200; t += 20) {
        TEST_ASSERT_FALSE(shake_update(&s, 0.0f, 0.0f, 2.2f, t));
    }
}

static void test_all_three_axes_count(void)
{
    shake_t s;
    shake_reset(&s);
    /* 1.3 g on each axis is 2.25 g in all */
    TEST_ASSERT_FALSE(shake_update(&s, 1.3f, -1.3f, 1.3f, 0));
    TEST_ASSERT_FALSE(shake_update(&s, -1.3f, 1.3f, -1.3f, 20));
    TEST_ASSERT_TRUE(shake_update(&s, 1.3f, 1.3f, 1.3f, 40));
}

static void test_lockout_of_1500ms_after_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    hit(&s, 0);
    hit(&s, 20);
    TEST_ASSERT_TRUE(hit(&s, 40));
    for (uint32_t t = 60; t < 1540; t += 20) {
        TEST_ASSERT_FALSE(hit(&s, t)); /* still shaking, but locked out */
    }
    TEST_ASSERT_FALSE(hit(&s, 1540)); /* 1500 ms later: counts again, from zero */
    TEST_ASSERT_FALSE(hit(&s, 1560));
    TEST_ASSERT_TRUE(hit(&s, 1580));
}

static void test_shake_survives_the_millisecond_counter_wrapping(void)
{
    shake_t s;
    shake_reset(&s);
    hit(&s, 0xFFFFFFF0u);
    hit(&s, 0x00000004u);
    TEST_ASSERT_TRUE(hit(&s, 0x00000018u));  /* 40 ms after the first */
    TEST_ASSERT_FALSE(hit(&s, 0x00000100u)); /* locked out */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_three_strong_samples_within_300ms_are_a_shake);
    RUN_TEST(test_a_jolt_of_two_strong_samples_is_not_a_shake);
    RUN_TEST(test_samples_spread_over_more_than_300ms_are_not_a_shake);
    RUN_TEST(test_exactly_2_2g_does_not_count);
    RUN_TEST(test_all_three_axes_count);
    RUN_TEST(test_lockout_of_1500ms_after_a_shake);
    RUN_TEST(test_shake_survives_the_millisecond_counter_wrapping);
    return UNITY_END();
}
