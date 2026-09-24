#include "brush.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static stamp_t st[BRUSH_MAX_STAMPS];

static void test_a_touch_paints_one_dot_where_it_lands(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    TEST_ASSERT_EQUAL(1, brush_down(&b, 233, 233, st));
    TEST_ASSERT_EQUAL_INT16(233, st[0].x);
    TEST_ASSERT_EQUAL_INT16(233, st[0].y);
    TEST_ASSERT_EQUAL_UINT8(0, st[0].color);
}

static void test_dots_come_every_7px_even_between_samples(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 233, st);
    TEST_ASSERT_EQUAL(10, brush_move(&b, 303, 233, st)); /* one 70 px sample */
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT16(240 + 7 * i, st[i].x);
        TEST_ASSERT_EQUAL_INT16(233, st[i].y);
    }
}

static void test_the_color_steps_every_3_dots_and_wraps_after_7(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 100, st); /* dot 0 */
    size_t n = brush_move(&b, 233, 100 + 7 * 21, st); /* dots 1..21 */
    TEST_ASSERT_EQUAL(21, n);
    TEST_ASSERT_EQUAL_UINT8(0, st[1].color);  /* dot 2 */
    TEST_ASSERT_EQUAL_UINT8(1, st[2].color);  /* dot 3 */
    TEST_ASSERT_EQUAL_UINT8(6, st[19].color); /* dot 20 */
    TEST_ASSERT_EQUAL_UINT8(0, st[20].color); /* dot 21 wraps */
}

static void test_short_moves_add_up(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 233, st);
    TEST_ASSERT_EQUAL(0, brush_move(&b, 236, 233, st));
    TEST_ASSERT_EQUAL(0, brush_move(&b, 239, 233, st));
    TEST_ASSERT_EQUAL(1, brush_move(&b, 241, 233, st));
    TEST_ASSERT_EQUAL_INT16(240, st[0].x);
}

static void test_nothing_is_painted_outside_the_round_area(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    TEST_ASSERT_EQUAL(1, brush_down(&b, 438, 233, st));    /* 205 px out: inside */
    TEST_ASSERT_EQUAL(1, brush_move(&b, 460, 233, st));    /* 445 is inside, 452 and 459 are not */
    TEST_ASSERT_EQUAL_INT16(445, st[0].x);
    TEST_ASSERT_EQUAL(0, brush_down(&b, 20, 20, st));      /* a touch near the rim paints nothing */
    TEST_ASSERT_EQUAL_UINT32(2, b.dots);                   /* only painted dots count for color */
}

static void test_moving_without_a_touch_paints_nothing(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    TEST_ASSERT_EQUAL(0, brush_move(&b, 300, 233, st));
    brush_down(&b, 233, 233, st);
    brush_up(&b);
    TEST_ASSERT_EQUAL(0, brush_move(&b, 300, 233, st));
}

static void test_a_huge_jump_is_capped_and_the_stroke_goes_on_from_the_finger(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 30, st);
    size_t n = brush_move(&b, 233, 1030, st); /* 1000 px in one sample */
    TEST_ASSERT_TRUE(n <= BRUSH_MAX_STAMPS);
    TEST_ASSERT_EQUAL_FLOAT(1030.0f, b.ly);
}

static void test_paw_prints_alternate_sides_and_turn_along_the_stroke(void)
{
    brush_t b;
    brush_init(&b, BRUSH_PAWS, 233, 233);
    TEST_ASSERT_EQUAL(1, brush_down(&b, 233, 233, st)); /* first print: toes up, to the left */
    TEST_ASSERT_EQUAL_INT16(224, st[0].x);
    TEST_ASSERT_EQUAL_INT16(233, st[0].y);
    TEST_ASSERT_EQUAL_INT16(0, st[0].angle_deg);
    TEST_ASSERT_EQUAL(2, brush_move(&b, 333, 233, st)); /* moving right: prints at 40 and 80 px */
    TEST_ASSERT_EQUAL_INT16(90, st[0].angle_deg);       /* toes point right */
    TEST_ASSERT_EQUAL_INT16(273, st[0].x);
    TEST_ASSERT_EQUAL_INT16(242, st[0].y);               /* right-hand side of the stroke */
    TEST_ASSERT_EQUAL_INT16(313, st[1].x);
    TEST_ASSERT_EQUAL_INT16(224, st[1].y);               /* then the left */
    TEST_ASSERT_EQUAL_UINT8(1, st[0].color);
    TEST_ASSERT_EQUAL_UINT8(2, st[1].color);
}

static void test_clearing_starts_the_colors_over(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 100, st);
    brush_move(&b, 233, 170, st); /* 10 more dots */
    brush_up(&b);
    brush_clear(&b);
    brush_down(&b, 233, 233, st);
    TEST_ASSERT_EQUAL_UINT8(0, st[0].color);
}

static void test_paw_blobs_turn_with_the_print(void)
{
    blob_t bl[5];
    const stamp_t up = {100, 100, 0, 0};
    paw_blobs(&up, bl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, bl[0].x); /* pad below the middle */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 104.96f, bl[0].y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.93f, bl[2].y); /* toes above */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.0f, bl[0].r);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.4f, bl[1].r);
    const stamp_t right = {100, 100, 0, 90};
    paw_blobs(&right, bl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 95.04f, bl[0].x); /* pad behind, to the left */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, bl[0].y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 103.54f, bl[1].x); /* toes ahead, to the right */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.08f, bl[1].y);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_touch_paints_one_dot_where_it_lands);
    RUN_TEST(test_dots_come_every_7px_even_between_samples);
    RUN_TEST(test_the_color_steps_every_3_dots_and_wraps_after_7);
    RUN_TEST(test_short_moves_add_up);
    RUN_TEST(test_nothing_is_painted_outside_the_round_area);
    RUN_TEST(test_moving_without_a_touch_paints_nothing);
    RUN_TEST(test_a_huge_jump_is_capped_and_the_stroke_goes_on_from_the_finger);
    RUN_TEST(test_paw_prints_alternate_sides_and_turn_along_the_stroke);
    RUN_TEST(test_clearing_starts_the_colors_over);
    RUN_TEST(test_paw_blobs_turn_with_the_print);
    return UNITY_END();
}
