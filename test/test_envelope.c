#include "envelope.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_peak(void)
{
    const int16_t s[] = {5, -32768, 100};
    TEST_ASSERT_EQUAL_UINT16(32767, envelope_peak(s, 3));
    const int16_t q[] = {-7, 3};
    TEST_ASSERT_EQUAL_UINT16(7, envelope_peak(q, 2));
    TEST_ASSERT_EQUAL_UINT16(0, envelope_peak(q, 0));
}

static void test_mouth_thresholds(void)
{
    TEST_ASSERT_EQUAL_UINT8(0, envelope_mouth(0));
    TEST_ASSERT_EQUAL_UINT8(0, envelope_mouth(1499));
    TEST_ASSERT_EQUAL_UINT8(1, envelope_mouth(1500));
    TEST_ASSERT_EQUAL_UINT8(1, envelope_mouth(5999));
    TEST_ASSERT_EQUAL_UINT8(2, envelope_mouth(6000));
}

static void test_never_stops_before_speech(void)
{
    rec_stop_t r;
    rec_stop_init(&r);
    for (int i = 0; i < 300; i++) {
        TEST_ASSERT_FALSE(rec_stop_update(&r, 100, 480, 24000));
    }
    TEST_ASSERT_FALSE(r.heard);
}

static void test_stops_after_1200ms_of_quiet_following_speech(void)
{
    rec_stop_t r;
    rec_stop_init(&r);
    TEST_ASSERT_FALSE(rec_stop_update(&r, 5000, 480, 24000));
    for (int i = 0; i < 59; i++) {
        TEST_ASSERT_FALSE(rec_stop_update(&r, 200, 480, 24000));
    }
    TEST_ASSERT_TRUE(rec_stop_update(&r, 200, 480, 24000)); /* 60 x 20 ms = 1.2 s */
}

static void test_speech_resets_the_quiet_run(void)
{
    rec_stop_t r;
    rec_stop_init(&r);
    rec_stop_update(&r, 5000, 480, 24000);
    for (int i = 0; i < 50; i++) {
        rec_stop_update(&r, 200, 480, 24000);
    }
    TEST_ASSERT_FALSE(rec_stop_update(&r, 3000, 480, 24000));
    for (int i = 0; i < 59; i++) {
        TEST_ASSERT_FALSE(rec_stop_update(&r, 200, 480, 24000));
    }
    TEST_ASSERT_TRUE(rec_stop_update(&r, 200, 480, 24000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_peak);
    RUN_TEST(test_mouth_thresholds);
    RUN_TEST(test_never_stops_before_speech);
    RUN_TEST(test_stops_after_1200ms_of_quiet_following_speech);
    RUN_TEST(test_speech_resets_the_quiet_run);
    return UNITY_END();
}
