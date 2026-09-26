#include <string.h>
#include "batlog.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static batlog_t s_log; /* 2 KB: static, not on the test's stack */

static batlog_rec_t rec(uint32_t time)
{
    return (batlog_rec_t){.time = time, .mv = 3900, .pct = 80, .event = BATLOG_AWAKE};
}

static void test_an_empty_log_has_nothing(void)
{
    batlog_clear(&s_log);
    batlog_rec_t r;
    TEST_ASSERT_FALSE(batlog_get(&s_log, 0, &r));
    TEST_ASSERT_TRUE(batlog_valid(&s_log));
}

static void test_records_come_back_oldest_first(void)
{
    batlog_clear(&s_log);
    for (uint32_t t = 1; t <= 3; t++) {
        batlog_push(&s_log, rec(t));
    }
    batlog_rec_t r;
    for (uint32_t i = 0; i < 3; i++) {
        TEST_ASSERT_TRUE(batlog_get(&s_log, i, &r));
        TEST_ASSERT_EQUAL_UINT32(i + 1, r.time);
    }
    TEST_ASSERT_FALSE(batlog_get(&s_log, 3, &r));
    TEST_ASSERT_TRUE(batlog_valid(&s_log));
}

static void test_a_full_log_keeps_all_of_them(void)
{
    batlog_clear(&s_log);
    for (uint32_t t = 1; t <= BATLOG_CAP; t++) {
        batlog_push(&s_log, rec(t));
    }
    batlog_rec_t r;
    TEST_ASSERT_TRUE(batlog_get(&s_log, 0, &r));
    TEST_ASSERT_EQUAL_UINT32(1, r.time);
    TEST_ASSERT_TRUE(batlog_get(&s_log, BATLOG_CAP - 1, &r));
    TEST_ASSERT_EQUAL_UINT32(BATLOG_CAP, r.time);
    TEST_ASSERT_FALSE(batlog_get(&s_log, BATLOG_CAP, &r));
    TEST_ASSERT_TRUE(batlog_valid(&s_log));
}

static void test_past_full_the_oldest_are_replaced(void)
{
    batlog_clear(&s_log);
    for (uint32_t t = 1; t <= BATLOG_CAP + 5; t++) {
        batlog_push(&s_log, rec(t));
    }
    batlog_rec_t r;
    TEST_ASSERT_EQUAL_UINT16(BATLOG_CAP, s_log.count);
    TEST_ASSERT_TRUE(batlog_get(&s_log, 0, &r));
    TEST_ASSERT_EQUAL_UINT32(6, r.time); /* 1-5 were replaced */
    TEST_ASSERT_TRUE(batlog_get(&s_log, BATLOG_CAP - 1, &r));
    TEST_ASSERT_EQUAL_UINT32(BATLOG_CAP + 5, r.time);
    TEST_ASSERT_TRUE(batlog_valid(&s_log));
}

static void test_indexes_out_of_range_are_not_valid(void)
{
    batlog_clear(&s_log);
    s_log.next = BATLOG_CAP;
    TEST_ASSERT_FALSE(batlog_valid(&s_log));
    batlog_clear(&s_log);
    s_log.count = BATLOG_CAP + 1;
    TEST_ASSERT_FALSE(batlog_valid(&s_log));
    batlog_clear(&s_log);
    s_log.count = 3; /* not full, so the next record must go right after the last */
    s_log.next = 7;
    TEST_ASSERT_FALSE(batlog_valid(&s_log));
}

static void test_a_record_formats_as_one_csv_line(void)
{
    char line[BATLOG_LINE_MAX];
    const batlog_rec_t r = {.time = 1790000000, .mv = 3948, .pct = 34, .event = BATLOG_SLEEP};
    const int n = batlog_format(&r, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("BATLOG,1790000000,34,3948,sleep", line);
    TEST_ASSERT_EQUAL_INT((int)strlen(line), n);
}

static void test_no_gauge_reading_formats_as_a_dash(void)
{
    char line[BATLOG_LINE_MAX];
    const batlog_rec_t r = {.time = 12, .mv = 0, .pct = BATLOG_NO_PCT, .event = BATLOG_COLD};
    batlog_format(&r, line, sizeof(line));
    TEST_ASSERT_EQUAL_STRING("BATLOG,12,-,0,cold", line);
}

static void test_the_longest_line_fits(void)
{
    char line[BATLOG_LINE_MAX];
    const batlog_rec_t r = {.time = UINT32_MAX, .mv = UINT16_MAX, .pct = 100, .event = BATLOG_WAKE_TIMER};
    const int n = batlog_format(&r, line, sizeof(line));
    TEST_ASSERT_LESS_THAN_INT(BATLOG_LINE_MAX, n);
}

static void test_every_event_has_a_name(void)
{
    for (uint8_t e = 0; e < BATLOG_EVENT_COUNT; e++) {
        TEST_ASSERT_NOT_NULL(batlog_event_name(e));
        TEST_ASSERT_NOT_EQUAL(0, strcmp("?", batlog_event_name(e)));
    }
    TEST_ASSERT_EQUAL_STRING("?", batlog_event_name(BATLOG_EVENT_COUNT));
    TEST_ASSERT_EQUAL_STRING("usb_out", batlog_event_name(BATLOG_USB_OUT));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_an_empty_log_has_nothing);
    RUN_TEST(test_records_come_back_oldest_first);
    RUN_TEST(test_a_full_log_keeps_all_of_them);
    RUN_TEST(test_past_full_the_oldest_are_replaced);
    RUN_TEST(test_indexes_out_of_range_are_not_valid);
    RUN_TEST(test_a_record_formats_as_one_csv_line);
    RUN_TEST(test_no_gauge_reading_formats_as_a_dash);
    RUN_TEST(test_the_longest_line_fits);
    RUN_TEST(test_every_event_has_a_name);
    return UNITY_END();
}
