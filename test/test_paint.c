#include <stdbool.h>
#include <stddef.h>
#include "paint.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define W     40
#define H     30
#define GUARD 64
static uint16_t s_buf[GUARD + W * H + GUARD];
static uint16_t *const px = s_buf + GUARD;

static void fill(uint16_t v)
{
    for (size_t i = 0; i < sizeof(s_buf) / sizeof(s_buf[0]); i++) {
        s_buf[i] = 0xBEEF; /* guard value outside the picture */
    }
    for (int i = 0; i < W * H; i++) {
        px[i] = v;
    }
}

static bool guards_intact(void)
{
    for (int i = 0; i < GUARD; i++) {
        if (s_buf[i] != 0xBEEF || s_buf[GUARD + W * H + i] != 0xBEEF) {
            return false;
        }
    }
    return true;
}

static uint16_t at(int x, int y) { return px[y * W + x]; }

static void test_rgb565_packs_the_design_colors(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xF800, paint_rgb565(0xFF0000));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, paint_rgb565(0x00FF00));
    TEST_ASSERT_EQUAL_HEX16(0x001F, paint_rgb565(0x0000FF));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, paint_rgb565(0xFFFFFF));
    TEST_ASSERT_EQUAL_HEX16(0xFACB, paint_rgb565(0xFF5A5A)); /* rainbow r1 */
}

static void test_a_solid_dot_covers_its_middle_and_nothing_beyond_its_edge(void)
{
    fill(0x0000);
    paint_circle(px, W, H, 20.5f, 15.5f, 8.0f, 0xFFFF, 255, NULL); /* centered on pixel (20, 15) */
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(20, 15));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(27, 15)); /* 7 px out */
    TEST_ASSERT_EQUAL_HEX16(0x0000, at(29, 15)); /* 9 px out */
    TEST_ASSERT_EQUAL_HEX16(0x0000, at(20, 6));
    TEST_ASSERT_TRUE(guards_intact());
}

static void test_the_edge_is_antialiased(void)
{
    fill(0x0000);
    paint_circle(px, W, H, 20.5f, 15.5f, 8.0f, 0xFFFF, 255, NULL);
    TEST_ASSERT_EQUAL_HEX16(0x7BEF, at(28, 15)); /* 8 px out, right on the edge: half covered */
}

static void test_opacity_blends_over_what_is_there(void)
{
    fill(0x0000);
    paint_circle(px, W, H, 20.0f, 15.0f, 15.0f, 0xFFFF, 72, NULL); /* a 28 % halo */
    TEST_ASSERT_EQUAL_HEX16(0x4228, at(20, 15));
    paint_circle(px, W, H, 20.0f, 15.0f, 8.0f, 0xF800, 255, NULL); /* then a solid red dot on top */
    TEST_ASSERT_EQUAL_HEX16(0xF800, at(20, 15));
}

static void test_painting_is_clipped_to_the_picture(void)
{
    fill(0x0000);
    paint_circle(px, W, H, -3.0f, -3.0f, 8.0f, 0xFFFF, 255, NULL);  /* over the top-left corner */
    paint_circle(px, W, H, 43.0f, 33.0f, 8.0f, 0xFFFF, 255, NULL);  /* over the bottom-right corner */
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(0, 0));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(W - 1, H - 1));
    TEST_ASSERT_TRUE(guards_intact());
}

static void test_the_dirty_rect_covers_what_changed(void)
{
    fill(0x0000);
    paint_rect_t d;
    paint_rect_reset(&d);
    paint_circle(px, W, H, -50.0f, -50.0f, 8.0f, 0xFFFF, 255, &d); /* entirely outside: nothing */
    TEST_ASSERT_TRUE(d.x1 < d.x0);
    paint_circle(px, W, H, 20.0f, 15.0f, 8.0f, 0xFFFF, 255, &d);
    TEST_ASSERT_EQUAL_INT16(12, d.x0);
    TEST_ASSERT_EQUAL_INT16(27, d.x1);
    TEST_ASSERT_EQUAL_INT16(7, d.y0);
    TEST_ASSERT_EQUAL_INT16(22, d.y1);
    paint_circle(px, W, H, 38.0f, 2.0f, 4.0f, 0xFFFF, 255, &d); /* grows, clipped at the edges */
    TEST_ASSERT_EQUAL_INT16(12, d.x0);
    TEST_ASSERT_EQUAL_INT16(W - 1, d.x1);
    TEST_ASSERT_EQUAL_INT16(0, d.y0);
    TEST_ASSERT_EQUAL_INT16(22, d.y1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rgb565_packs_the_design_colors);
    RUN_TEST(test_a_solid_dot_covers_its_middle_and_nothing_beyond_its_edge);
    RUN_TEST(test_the_edge_is_antialiased);
    RUN_TEST(test_opacity_blends_over_what_is_there);
    RUN_TEST(test_painting_is_clipped_to_the_picture);
    RUN_TEST(test_the_dirty_rect_covers_what_changed);
    return UNITY_END();
}
