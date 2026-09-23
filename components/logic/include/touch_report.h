#pragma once
#include <stddef.h>
#include <stdint.h>

/* The CST9217 touch report at register 0xD000: 5 bytes per point (point i starts at i * 5, plus 2
 * after the first), the reported finger count in byte 5 and an ACK byte (0xAB) in byte 6. The
 * Waveshare driver reads only one point, so the parent gesture reads the report itself. */
#define TOUCH_REPORT_REG 0xD000
#define TOUCH_REPORT_ACK 0xAB
#define TOUCH_REPORT_LEN 15 /* two points */

/* Writes the touching points to x/y (raw controller coordinates, not mirrored) and returns how
 * many there are (0..2). A report without the ACK byte has none. */
uint8_t touch_report_parse(const uint8_t *d, size_t len, int16_t x[2], int16_t y[2]);
