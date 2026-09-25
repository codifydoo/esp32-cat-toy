#include "unity.h"
#include "yarn_rules.h"

void setUp(void) {}
void tearDown(void) {}

static void test_the_four_basket_spots_come_round_again(void)
{
    int16_t x, y;
    yarn_basket(0, &x, &y);
    TEST_ASSERT_EQUAL_INT16(330, x);
    TEST_ASSERT_EQUAL_INT16(318, y);
    yarn_basket(3, &x, &y);
    TEST_ASSERT_EQUAL_INT16(136, x);
    TEST_ASSERT_EQUAL_INT16(150, y);
    yarn_basket(5, &x, &y); /* wraps to spot 1 */
    TEST_ASSERT_EQUAL_INT16(136, x);
    TEST_ASSERT_EQUAL_INT16(318, y);
}

static void test_the_cat_follows_on_the_center_side(void)
{
    float tx, ty;
    yarn_chase_target(333.0f, 233.0f, 233.0f, 233.0f, &tx, &ty); /* yarn to the right */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 241.0f, tx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 233.0f, ty);
    yarn_chase_target(233.0f, 133.0f, 233.0f, 233.0f, &tx, &ty); /* yarn above */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 233.0f, tx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 225.0f, ty);
}

static void test_near_the_center_the_cat_waits_above_the_yarn(void)
{
    float tx, ty;
    yarn_chase_target(240.0f, 233.0f, 233.0f, 233.0f, &tx, &ty);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 240.0f, tx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 141.0f, ty);
}

static void test_in_the_basket_within_62px_of_its_mouth(void)
{
    /* basket 0 at (330, 318): its mouth is (330, 308) */
    TEST_ASSERT_TRUE(yarn_in_basket(330.0f, 308.0f, 330, 318));
    TEST_ASSERT_TRUE(yarn_in_basket(330.0f, 369.9f, 330, 318));
    TEST_ASSERT_FALSE(yarn_in_basket(330.0f, 370.0f, 330, 318));
    TEST_ASSERT_FALSE(yarn_in_basket(233.0f, 233.0f, 330, 318)); /* the start, in the middle */
}

static void test_a_new_round_never_starts_in_a_basket(void)
{
    for (uint8_t i = 0; i < YARN_BASKETS; i++) {
        int16_t x, y;
        yarn_basket(i, &x, &y);
        TEST_ASSERT_FALSE(yarn_in_basket(233.0f, 233.0f, x, y)); /* the yarn restarts in the middle */
    }
}

static void test_the_cat_eases_toward_its_spot(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, yarn_follow(10.0f, 110.0f, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 73.21f, yarn_follow(10.0f, 110.0f, YARN_FOLLOW_MS)); /* 63 % of the gap */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 110.0f, yarn_follow(10.0f, 110.0f, 2000));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -53.21f, yarn_follow(10.0f, -90.0f, YARN_FOLLOW_MS)); /* either way */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_four_basket_spots_come_round_again);
    RUN_TEST(test_the_cat_follows_on_the_center_side);
    RUN_TEST(test_near_the_center_the_cat_waits_above_the_yarn);
    RUN_TEST(test_in_the_basket_within_62px_of_its_mouth);
    RUN_TEST(test_a_new_round_never_starts_in_a_basket);
    RUN_TEST(test_the_cat_eases_toward_its_spot);
    return UNITY_END();
}
