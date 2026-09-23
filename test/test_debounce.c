#include "debounce.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_press_needs_two_samples_and_fires_once(void)
{
    debounce_t d;
    debounce_init(&d, false);
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_TRUE(debounce_update(&d, true));
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_FALSE(debounce_update(&d, true));
}

static void test_single_sample_glitch_is_ignored(void)
{
    debounce_t d;
    debounce_init(&d, false);
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_FALSE(debounce_update(&d, false));
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_TRUE(debounce_update(&d, true));
}

static void test_release_then_press_fires_again(void)
{
    debounce_t d;
    debounce_init(&d, false);
    debounce_update(&d, true);
    debounce_update(&d, true);
    TEST_ASSERT_FALSE(debounce_update(&d, false));
    TEST_ASSERT_FALSE(debounce_update(&d, false)); /* release is not reported */
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_TRUE(debounce_update(&d, true));
}

static void test_button_held_at_boot_does_not_fire(void)
{
    debounce_t d;
    debounce_init(&d, true);
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_FALSE(debounce_update(&d, false));
    TEST_ASSERT_FALSE(debounce_update(&d, false));
    TEST_ASSERT_FALSE(debounce_update(&d, true));
    TEST_ASSERT_TRUE(debounce_update(&d, true));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_press_needs_two_samples_and_fires_once);
    RUN_TEST(test_single_sample_glitch_is_ignored);
    RUN_TEST(test_release_then_press_fires_again);
    RUN_TEST(test_button_held_at_boot_does_not_fire);
    return UNITY_END();
}
