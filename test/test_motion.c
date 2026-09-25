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

/* ---- tilt ---- */

#define TOL 0.05f

static void settle(tilt_t *t, float x, float y, float z)
{
    for (int i = 0; i < 80; i++) {
        tilt_feed(t, x, y, z);
    }
}

/* The accelerometer's reading for a toy tipped back by pitch (top edge up, as when she holds it
 * toward her face) and then turned by roll about its own screen y axis (right edge down). */
static void up_vector(float pitch_deg, float roll_deg, float v[3])
{
    const float p = pitch_deg * 3.14159265f / 180.0f, r = roll_deg * 3.14159265f / 180.0f;
    v[0] = -sinf(r) * cosf(p);
    v[1] = -sinf(p);
    v[2] = cosf(r) * cosf(p);
}

static void hold_at(tilt_t *t, float pitch_deg, float roll_deg)
{
    float v[3];
    up_vector(pitch_deg, roll_deg, v);
    settle(t, v[0], v[1], v[2]);
}

static void read_angles(const tilt_t *t, float *x, float *y) { TEST_ASSERT_TRUE(tilt_angles(t, x, y)); }

static void test_no_reading_no_angles(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    TEST_ASSERT_FALSE(tilt_angles(&t, &x, &y));
}

static void test_without_a_grip_flat_is_level(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    hold_at(&t, 0, 10); /* right edge 10° down */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 10.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, y);
    hold_at(&t, 10, 0); /* top edge 10° up: the bottom edge is lower */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 10.0f, y);
}

static void test_the_grip_at_recenter_is_level(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    hold_at(&t, 40, 0); /* tipped toward her face */
    tilt_recenter(&t);
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, y);
    hold_at(&t, 50, 0); /* tipped 10° further */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 10.0f, y);
    hold_at(&t, 0, 0); /* laid flat: the top edge is now 40° lower than at the grip */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, -40.0f, y);
}

static void test_turning_at_the_grip_rolls_sideways(void)
{
    /* 15° about the toy's own tipped axis moves "down" by 11.4° (that axis is not horizontal),
     * and a little along y: the angles measure how far down moved, like a marble would. */
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    hold_at(&t, 40, 0);
    tilt_recenter(&t);
    hold_at(&t, 40, -15); /* left edge down */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, -11.44f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.98f, y);
}

static void test_recenter_before_any_reading_uses_the_first_one(void)
{
    tilt_t t;
    tilt_reset(&t);
    tilt_recenter(&t);
    float v[3], x, y;
    up_vector(40, 0, v);
    tilt_feed(&t, v[0], v[1], v[2]);
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, y);
}

static void test_one_bump_stays_inside_the_dead_zone(void)
{
    /* A single 30° sample (a knock, setting it down) is smoothed to 7.4°, under the 8° dead zone. */
    tilt_t t;
    tilt_reset(&t);
    float v[3], x, y;
    hold_at(&t, 0, 0);
    up_vector(0, 30, v);
    tilt_feed(&t, v[0], v[1], v[2]);
    read_angles(&t, &x, &y);
    TEST_ASSERT_TRUE(tilt_is_level(x, y));
}

static void test_one_sideways_spike_of_half_a_g_stays_inside_the_dead_zone(void)
{
    /* A knock is an acceleration spike, not a rotation: one sample with 0.5 g sideways on top of
     * gravity is smoothed to 7.1°. (0.6 g would read 8.5°: harder knocks are an M8 tuning item.) */
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    hold_at(&t, 0, 0);
    tilt_feed(&t, 0.5f, 0.0f, 1.0f);
    read_angles(&t, &x, &y);
    TEST_ASSERT_TRUE(tilt_is_level(x, y));
}

static void test_a_face_down_grip_gives_finite_angles(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    settle(&t, 0.0f, 0.0f, -1.0f);
    tilt_recenter(&t);
    settle(&t, 0.1f, -0.1f, -0.99f);
    read_angles(&t, &x, &y);
    TEST_ASSERT_TRUE(isfinite(x) && isfinite(y));
}

static void test_dead_zone_and_speed_curve(void)
{
    float vx, vy;
    tilt_speed(8.0f, 0.0f, &vx, &vy);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, vx);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, vy);
    tilt_speed(16.5f, 0.0f, &vx, &vy); /* halfway from 8° to 25° */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, vx);
    tilt_speed(25.0f, 0.0f, &vx, &vy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 240.0f, vx);
    tilt_speed(0.0f, -60.0f, &vx, &vy); /* capped, toward the top */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, vx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -240.0f, vy);
}

static void test_speed_follows_the_tilt_direction(void)
{
    float vx, vy;
    tilt_speed(17.67767f, 17.67767f, &vx, &vy); /* 25° toward the bottom right */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 169.71f, vx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 169.71f, vy);
    TEST_ASSERT_TRUE(tilt_is_level(5.0f, 6.0f));  /* 7.8°: the dead zone is round */
    TEST_ASSERT_FALSE(tilt_is_level(6.0f, 6.0f)); /* 8.5° */
}

static void test_roll_moves_by_speed_times_time(void)
{
    float x = 233.0f, y = 233.0f;
    float moved = roll_step(&x, &y, 25.0f, 0.0f, 100, 233.0f, 233.0f, 190.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.0f, moved);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 257.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 233.0f, y);
}

static void test_level_does_not_roll(void)
{
    float x = 300.0f, y = 200.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, roll_step(&x, &y, 5.0f, -5.0f, 20, 233.0f, 233.0f, 190.0f));
    TEST_ASSERT_EQUAL_FLOAT(300.0f, x);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, y);
}

static void test_the_ball_stays_inside_the_rim(void)
{
    float x = 233.0f + 185.0f, y = 233.0f;
    float moved = roll_step(&x, &y, 25.0f, 0.0f, 100, 233.0f, 233.0f, 190.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 423.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, moved);
    /* pushed straight into the rim it stays put, and that is not movement (so not activity) */
    moved = roll_step(&x, &y, 25.0f, 0.0f, 100, 233.0f, 233.0f, 190.0f);
    TEST_ASSERT_TRUE(moved < 0.01f);
}

static void test_the_ball_slides_along_the_rim(void)
{
    float x = 423.0f, y = 233.0f;
    roll_step(&x, &y, 0.0f, 25.0f, 100, 233.0f, 233.0f, 190.0f); /* tilted toward the bottom */
    TEST_ASSERT_TRUE(y > 250.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 190.0f, hypotf(x - 233.0f, y - 233.0f));
}

static void test_a_stall_rolls_at_most_100ms(void)
{
    float x = 233.0f, y = 233.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.0f, roll_step(&x, &y, 25.0f, 0.0f, 5000, 233.0f, 233.0f, 190.0f));
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
    RUN_TEST(test_no_reading_no_angles);
    RUN_TEST(test_without_a_grip_flat_is_level);
    RUN_TEST(test_the_grip_at_recenter_is_level);
    RUN_TEST(test_turning_at_the_grip_rolls_sideways);
    RUN_TEST(test_recenter_before_any_reading_uses_the_first_one);
    RUN_TEST(test_one_bump_stays_inside_the_dead_zone);
    RUN_TEST(test_one_sideways_spike_of_half_a_g_stays_inside_the_dead_zone);
    RUN_TEST(test_a_face_down_grip_gives_finite_angles);
    RUN_TEST(test_dead_zone_and_speed_curve);
    RUN_TEST(test_speed_follows_the_tilt_direction);
    RUN_TEST(test_roll_moves_by_speed_times_time);
    RUN_TEST(test_level_does_not_roll);
    RUN_TEST(test_the_ball_stays_inside_the_rim);
    RUN_TEST(test_the_ball_slides_along_the_rim);
    RUN_TEST(test_a_stall_rolls_at_most_100ms);
    return UNITY_END();
}
