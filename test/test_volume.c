#include "unity.h"
#include "volume.h"

void setUp(void) {}
void tearDown(void) {}

static void test_steps_by_ten(void)
{
    TEST_ASSERT_EQUAL_UINT8(70, volume_step(60, +1));
    TEST_ASSERT_EQUAL_UINT8(50, volume_step(60, -1));
}

static void test_stays_within_ten_and_a_hundred(void)
{
    TEST_ASSERT_EQUAL_UINT8(100, volume_step(100, +1));
    TEST_ASSERT_EQUAL_UINT8(10, volume_step(10, -1));
}

static void test_odd_values_snap_to_the_next_step(void)
{
    TEST_ASSERT_EQUAL_UINT8(70, volume_step(65, +1));
    TEST_ASSERT_EQUAL_UINT8(60, volume_step(65, -1));
}

static void test_out_of_range_values_come_back_in(void)
{
    TEST_ASSERT_EQUAL_UINT8(10, volume_step(0, +1));
    TEST_ASSERT_EQUAL_UINT8(10, volume_step(5, -1));
    TEST_ASSERT_EQUAL_UINT8(100, volume_step(255, +1));
    TEST_ASSERT_EQUAL_UINT8(100, volume_step(255, -1)); /* out-of-range input clamps to 100 first */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_steps_by_ten);
    RUN_TEST(test_stays_within_ten_and_a_hundred);
    RUN_TEST(test_odd_values_snap_to_the_next_step);
    RUN_TEST(test_out_of_range_values_come_back_in);
    return UNITY_END();
}
