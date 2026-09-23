#include <stdlib.h>
#include "unity.h"
#include "voice_fx.h"

void setUp(void) {}
void tearDown(void) {}

static void test_mouse_plays_1_5x_faster_with_interpolation(void)
{
    const int16_t in[7] = {0, 100, 200, 300, 400, 500, 600};
    int16_t out[8];
    TEST_ASSERT_EQUAL(5, voice_fx_output_len(VOICE_MOUSE, 7, 24000));
    TEST_ASSERT_EQUAL(5, voice_fx_apply(VOICE_MOUSE, in, 7, out, 8, 24000));
    const int16_t want[5] = {0, 150, 300, 450, 600};
    TEST_ASSERT_EQUAL_INT16_ARRAY(want, out, 5);
}

static void test_lion_plays_slower(void)
{
    const int16_t in[4] = {0, 300, 600, 900};
    int16_t out[8];
    TEST_ASSERT_EQUAL(5, voice_fx_output_len(VOICE_LION, 4, 24000));
    TEST_ASSERT_EQUAL(5, voice_fx_apply(VOICE_LION, in, 4, out, 8, 24000));
    const int16_t want[5] = {0, 200, 400, 600, 800};
    for (int i = 0; i < 5; i++) {
        TEST_ASSERT_INT_WITHIN(1, want[i], out[i]);
    }
}

static void test_lion_length_for_five_seconds(void)
{
    size_t n = voice_fx_output_len(VOICE_LION, 120000, 24000);
    TEST_ASSERT_TRUE(n >= 179990 && n <= 180000);
}

static void test_robot_is_bitcrushed_and_held(void)
{
    enum { N = 2400 };
    int16_t *in = malloc(N * sizeof(int16_t));
    int16_t *out = malloc(N * sizeof(int16_t));
    for (int i = 0; i < N; i++) {
        in[i] = 10000;
    }
    TEST_ASSERT_EQUAL(N, voice_fx_output_len(VOICE_ROBOT, N, 24000));
    TEST_ASSERT_EQUAL(N, voice_fx_apply(VOICE_ROBOT, in, N, out, N, 24000));
    TEST_ASSERT_EQUAL_INT16(0, out[0]); /* sin(0) = 0 */
    int16_t max = 0;
    for (int i = 0; i < N; i++) {
        TEST_ASSERT_EQUAL_HEX16(0, (uint16_t)out[i] & 0xFF);
        if (i % 2) {
            TEST_ASSERT_EQUAL_INT16(out[i - 1], out[i]);
        }
        if (abs(out[i]) > max) {
            max = (int16_t)abs(out[i]);
        }
    }
    TEST_ASSERT_TRUE(max >= 9000);
    free(in);
    free(out);
}

static void test_robot_keeps_silence_silent(void)
{
    int16_t in[64] = {0};
    int16_t out[64];
    voice_fx_apply(VOICE_ROBOT, in, 64, out, 64, 24000);
    for (int i = 0; i < 64; i++) {
        TEST_ASSERT_EQUAL_INT16(0, out[i]);
    }
}

static void test_cave_echoes_an_impulse(void)
{
    const int16_t in[1] = {10000};
    size_t n = voice_fx_output_len(VOICE_CAVE, 1, 24000);
    TEST_ASSERT_EQUAL(24001, n);
    int16_t *out = malloc(n * sizeof(int16_t));
    TEST_ASSERT_EQUAL(n, voice_fx_apply(VOICE_CAVE, in, 1, out, n, 24000));
    TEST_ASSERT_EQUAL_INT16(10000, out[0]);
    TEST_ASSERT_EQUAL_INT16(5000, out[6000]);
    TEST_ASSERT_EQUAL_INT16(2500, out[12000]);
    TEST_ASSERT_EQUAL_INT16(1250, out[18000]);
    TEST_ASSERT_EQUAL_INT16(625, out[24000]);
    TEST_ASSERT_EQUAL_INT16(0, out[1]);
    TEST_ASSERT_EQUAL_INT16(0, out[5999]);
    free(out);
}

static void test_cave_saturates_instead_of_wrapping(void)
{
    enum { N = 20000 };
    int16_t *in = malloc(N * sizeof(int16_t));
    for (int i = 0; i < N; i++) {
        in[i] = 30000;
    }
    size_t n = voice_fx_output_len(VOICE_CAVE, N, 24000);
    int16_t *out = malloc(n * sizeof(int16_t));
    voice_fx_apply(VOICE_CAVE, in, N, out, n, 24000);
    TEST_ASSERT_EQUAL_INT16(32767, out[18000]); /* 30000 + 15000 + 7500 + 3750 */
    free(in);
    free(out);
}

static void test_output_is_clipped_to_capacity(void)
{
    const int16_t in[7] = {0, 100, 200, 300, 400, 500, 600};
    int16_t out[3];
    TEST_ASSERT_EQUAL(3, voice_fx_apply(VOICE_MOUSE, in, 7, out, 3, 24000));
    TEST_ASSERT_EQUAL(0, voice_fx_apply(VOICE_MOUSE, in, 0, out, 3, 24000));
}

static void test_normalize_brings_speech_up_to_the_asset_level(void)
{
    int16_t buf[3] = {2000, -1000, 500};
    voice_fx_normalize(buf, 3);
    TEST_ASSERT_INT_WITHIN(2, VOICE_FX_TARGET_PEAK, buf[0]);
    TEST_ASSERT_INT_WITHIN(2, -VOICE_FX_TARGET_PEAK / 2, buf[1]);
    TEST_ASSERT_INT_WITHIN(2, VOICE_FX_TARGET_PEAK / 4, buf[2]);
}

static void test_normalize_limits_the_gain_on_a_quiet_take(void)
{
    int16_t buf[2] = {100, -50};
    voice_fx_normalize(buf, 2);
    TEST_ASSERT_EQUAL_INT16(100 * VOICE_FX_MAX_GAIN, buf[0]);
    TEST_ASSERT_EQUAL_INT16(-50 * VOICE_FX_MAX_GAIN, buf[1]);
}

static void test_normalize_turns_a_clipped_take_down(void)
{
    int16_t buf[3] = {INT16_MIN, INT16_MAX, 0};
    voice_fx_normalize(buf, 3);
    TEST_ASSERT_INT_WITHIN(2, -VOICE_FX_TARGET_PEAK, buf[0]);
    TEST_ASSERT_INT_WITHIN(2, VOICE_FX_TARGET_PEAK, buf[1]);
    TEST_ASSERT_EQUAL_INT16(0, buf[2]);
}

static void test_normalize_leaves_silence_alone(void)
{
    int16_t buf[3] = {0, 0, 0};
    voice_fx_normalize(buf, 3);
    const int16_t want[3] = {0, 0, 0};
    TEST_ASSERT_EQUAL_INT16_ARRAY(want, buf, 3);
    voice_fx_normalize(buf, 0);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_mouse_plays_1_5x_faster_with_interpolation);
    RUN_TEST(test_lion_plays_slower);
    RUN_TEST(test_lion_length_for_five_seconds);
    RUN_TEST(test_robot_is_bitcrushed_and_held);
    RUN_TEST(test_robot_keeps_silence_silent);
    RUN_TEST(test_cave_echoes_an_impulse);
    RUN_TEST(test_cave_saturates_instead_of_wrapping);
    RUN_TEST(test_output_is_clipped_to_capacity);
    RUN_TEST(test_normalize_brings_speech_up_to_the_asset_level);
    RUN_TEST(test_normalize_limits_the_gain_on_a_quiet_take);
    RUN_TEST(test_normalize_turns_a_clipped_take_down);
    RUN_TEST(test_normalize_leaves_silence_alone);
    return UNITY_END();
}
