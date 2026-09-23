#include <string.h>
#include "unity.h"
#include "wav.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t buf[256];
static size_t len;

static void put(const void *p, size_t n) { memcpy(buf + len, p, n); len += n; }
static void put32(uint32_t v) { uint8_t b[4] = {v & 0xFF, (v >> 8) & 0xFF, (v >> 16) & 0xFF, v >> 24}; put(b, 4); }
static void put16(uint16_t v) { uint8_t b[2] = {v & 0xFF, v >> 8}; put(b, 2); }

static void header(void)
{
    len = 0;
    put("RIFF", 4);
    put32(0);
    put("WAVE", 4);
}

static void fmt_chunk(uint16_t format, uint16_t ch, uint32_t rate, uint16_t bits)
{
    put("fmt ", 4);
    put32(16);
    put16(format);
    put16(ch);
    put32(rate);
    put32(rate * ch * bits / 8);
    put16((uint16_t)(ch * bits / 8));
    put16(bits);
}

static void data_chunk(uint32_t declared, size_t actual)
{
    put("data", 4);
    put32(declared);
    memset(buf + len, 0x11, actual);
    len += actual;
}

static void test_canonical_file(void)
{
    header();
    fmt_chunk(1, 1, 24000, 16);
    data_chunk(8, 8);
    wav_info_t w;
    TEST_ASSERT_TRUE(wav_parse(buf, len, &w));
    TEST_ASSERT_EQUAL_UINT16(1, w.channels);
    TEST_ASSERT_EQUAL_UINT32(24000, w.rate);
    TEST_ASSERT_EQUAL_UINT16(16, w.bits);
    TEST_ASSERT_EQUAL(44, w.data_offset);
    TEST_ASSERT_EQUAL(8, w.data_bytes);
}

static void test_skips_other_chunks_with_padding(void)
{
    header();
    fmt_chunk(1, 1, 24000, 16);
    put("LIST", 4);
    put32(3); /* odd size: one pad byte follows */
    put("abc", 3);
    put("\0", 1);
    data_chunk(4, 4);
    wav_info_t w;
    TEST_ASSERT_TRUE(wav_parse(buf, len, &w));
    TEST_ASSERT_EQUAL(56, w.data_offset);
    TEST_ASSERT_EQUAL(4, w.data_bytes);
}

static void test_truncated_data_is_clamped_to_even_bytes(void)
{
    header();
    fmt_chunk(1, 1, 24000, 16);
    data_chunk(1000, 7);
    wav_info_t w;
    TEST_ASSERT_TRUE(wav_parse(buf, len, &w));
    TEST_ASSERT_EQUAL(6, w.data_bytes);
}

static void test_rejects_huge_chunk_size(void)
{
    /* A non-data chunk declaring a size near UINT32_MAX: on a 32-bit size_t,
     * 8 + 0xFFFFFFF8 + 0 == 0x100000000, which wraps to 0, so `step` would be 0 and the
     * parser would loop on the same offset forever instead of rejecting the file. */
    wav_info_t w;
    header();
    fmt_chunk(1, 1, 24000, 16);
    put("JUNK", 4);
    put32(0xFFFFFFF8u);
    TEST_ASSERT_FALSE(wav_parse(buf, len, &w));
}

static void test_rejects_bad_files(void)
{
    wav_info_t w;
    header();
    fmt_chunk(3, 1, 24000, 32); /* float, not PCM */
    data_chunk(4, 4);
    TEST_ASSERT_FALSE(wav_parse(buf, len, &w));

    header();
    data_chunk(4, 4); /* no fmt */
    TEST_ASSERT_FALSE(wav_parse(buf, len, &w));

    memcpy(buf, "RIFX", 4);
    TEST_ASSERT_FALSE(wav_parse(buf, len, &w));
    TEST_ASSERT_FALSE(wav_parse(buf, 10, &w));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_canonical_file);
    RUN_TEST(test_skips_other_chunks_with_padding);
    RUN_TEST(test_truncated_data_is_clamped_to_even_bytes);
    RUN_TEST(test_rejects_huge_chunk_size);
    RUN_TEST(test_rejects_bad_files);
    return UNITY_END();
}
