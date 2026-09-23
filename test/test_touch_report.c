#include <string.h>
#include "touch_report.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* One point record: status 0x06 (touching), x/y as 12-bit values split across bytes 1-3. */
static void put_point(uint8_t *p, uint8_t status, uint16_t x, uint16_t y)
{
    p[0] = status;
    p[1] = (uint8_t)(x >> 4);
    p[2] = (uint8_t)(y >> 4);
    p[3] = (uint8_t)(((x & 0x0F) << 4) | (y & 0x0F));
    p[4] = 0;
}

static void report(uint8_t *d, uint8_t count)
{
    memset(d, 0, TOUCH_REPORT_LEN);
    d[5] = count;
    d[6] = TOUCH_REPORT_ACK;
}

static void test_two_fingers_give_both_points(void)
{
    uint8_t d[TOUCH_REPORT_LEN];
    report(d, 2);
    put_point(&d[0], 0x06, 100, 200);
    put_point(&d[7], 0x06, 300, 250);
    int16_t x[2], y[2];
    TEST_ASSERT_EQUAL_UINT8(2, touch_report_parse(d, sizeof(d), x, y));
    TEST_ASSERT_EQUAL_INT16(100, x[0]);
    TEST_ASSERT_EQUAL_INT16(200, y[0]);
    TEST_ASSERT_EQUAL_INT16(300, x[1]);
    TEST_ASSERT_EQUAL_INT16(250, y[1]);
}

static void test_one_finger_gives_one_point(void)
{
    uint8_t d[TOUCH_REPORT_LEN];
    report(d, 1);
    put_point(&d[0], 0x06, 233, 97);
    put_point(&d[7], 0x06, 1, 1); /* stale bytes after the reported count are ignored */
    int16_t x[2], y[2];
    TEST_ASSERT_EQUAL_UINT8(1, touch_report_parse(d, sizeof(d), x, y));
    TEST_ASSERT_EQUAL_INT16(233, x[0]);
    TEST_ASSERT_EQUAL_INT16(97, y[0]);
}

static void test_a_report_without_the_ack_byte_has_no_fingers(void)
{
    uint8_t d[TOUCH_REPORT_LEN];
    report(d, 2);
    put_point(&d[0], 0x06, 100, 200);
    put_point(&d[7], 0x06, 300, 250);
    d[6] = 0x00;
    int16_t x[2], y[2];
    TEST_ASSERT_EQUAL_UINT8(0, touch_report_parse(d, sizeof(d), x, y));
}

static void test_more_than_two_reported_counts_as_two(void)
{
    uint8_t d[TOUCH_REPORT_LEN];
    report(d, 5);
    put_point(&d[0], 0x06, 100, 200);
    put_point(&d[7], 0x06, 300, 250);
    int16_t x[2], y[2];
    TEST_ASSERT_EQUAL_UINT8(2, touch_report_parse(d, sizeof(d), x, y));
}

static void test_a_lifting_finger_is_not_counted(void)
{
    uint8_t d[TOUCH_REPORT_LEN];
    report(d, 2);
    put_point(&d[0], 0x00, 100, 200); /* released */
    put_point(&d[7], 0x06, 300, 250);
    int16_t x[2], y[2];
    TEST_ASSERT_EQUAL_UINT8(1, touch_report_parse(d, sizeof(d), x, y));
    TEST_ASSERT_EQUAL_INT16(300, x[0]);
    TEST_ASSERT_EQUAL_INT16(250, y[0]);
}

static void test_a_short_report_only_yields_what_it_holds(void)
{
    uint8_t d[TOUCH_REPORT_LEN];
    report(d, 2);
    put_point(&d[0], 0x06, 100, 200);
    put_point(&d[7], 0x06, 300, 250);
    int16_t x[2], y[2];
    TEST_ASSERT_EQUAL_UINT8(1, touch_report_parse(d, 10, x, y)); /* the driver's one-point length */
    TEST_ASSERT_EQUAL_UINT8(0, touch_report_parse(d, 6, x, y));  /* no ACK byte */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_two_fingers_give_both_points);
    RUN_TEST(test_one_finger_gives_one_point);
    RUN_TEST(test_a_report_without_the_ack_byte_has_no_fingers);
    RUN_TEST(test_more_than_two_reported_counts_as_two);
    RUN_TEST(test_a_lifting_finger_is_not_counted);
    RUN_TEST(test_a_short_report_only_yields_what_it_holds);
    return UNITY_END();
}
