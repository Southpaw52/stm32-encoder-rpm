/** @author southpaw | Project application code. */
/**
 * @file    telemetry.c
 * @brief   Bounded banner and measurement line formatting.
 * @details Runs in firmware main context and host tests without allocation or printf.
 *          A checked integer writer rejects overflow instead of sending truncated lines.
 */
#include "telemetry.h"

#include "encoder_config.h"
#include <stdbool.h>

#define DECIMAL_BASE             10U
#define UINT64_DECIMAL_DIGITS    20U

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    bool failed;
} line_writer_t;

/** @brief Append one character while reserving space for the terminating NUL. */
static void append_char(line_writer_t *writer, char value)
{
    if (writer->failed) {
        return;
    }
    if ((writer->buffer == NULL) || (writer->capacity == 0U)
        || (writer->length >= writer->capacity - 1U)) {
        writer->failed = true;
        return;
    }
    writer->buffer[writer->length++] = value;
}

/** @brief Append a NUL-terminated string through the bounded writer. */
static void append_text(line_writer_t *writer, const char *text)
{
    while (*text != '\0') {
        append_char(writer, *text++);
    }
}

/** @brief Append an unsigned decimal integer without allocation or printf. */
static void append_unsigned(line_writer_t *writer, uint64_t value)
{
    char digits[UINT64_DECIMAL_DIGITS];
    size_t count = 0U;
    do {
        digits[count++] = (char)('0' + (value % DECIMAL_BASE));
        value /= DECIMAL_BASE;
    } while (value != 0U);
    while (count != 0U) {
        append_char(writer, digits[--count]);
    }
}

/** @brief Append a signed decimal integer, including the INT64_MIN boundary. */
static void append_signed(line_writer_t *writer, int64_t value)
{
    uint64_t magnitude;
    if (value < 0) {
        append_char(writer, '-');
        /* Avoid negating INT64_MIN, including in intermediate expressions. */
        magnitude = (uint64_t)(-(value + 1)) + 1U;
    } else {
        if (value > 0) {
            append_char(writer, '+');
        }
        magnitude = (uint64_t)value;
    }
    append_unsigned(writer, magnitude);
}

/** @brief Finish a CRLF line with NUL, or return an empty string on overflow. */
static size_t finish_line(line_writer_t *writer)
{
    append_text(writer, "\r\n");
    if ((writer->buffer == NULL) || (writer->capacity == 0U)) {
        return 0U;
    }
    if (writer->failed) {
        writer->buffer[0] = '\0';
        return 0U;
    }
    writer->buffer[writer->length] = '\0';
    return writer->length;
}

size_t telemetry_format_banner(char *buf, size_t size)
{
    line_writer_t writer = {buf, size, 0U, false};
    append_text(&writer, "# encoder_rpm fw=" APP_FW_VERSION_STRING " ppr=");
    append_unsigned(&writer, ENCODER_PPR);
    append_text(&writer, " cpr=");
    append_unsigned(&writer, ENCODER_COUNTS_PER_REV);
    append_text(&writer, " window_ms=");
    append_unsigned(&writer, SPEED_WINDOW_MS);
    return finish_line(&writer);
}

size_t telemetry_format_sample(char *buf, size_t size, const speed_sample_t *s,
                                int32_t rpm_x100, encoder_direction_t dir,
                                uint32_t missed_windows)
{
    line_writer_t writer = {buf, size, 0U, false};
    append_text(&writer, "t=");
    append_unsigned(&writer, s->sequence);
    append_text(&writer, "s pulses=");
    append_signed(&writer, s->total_counts);
    append_text(&writer, " delta=");
    append_signed(&writer, s->delta_counts);
    append_text(&writer, " dir=");
    append_text(&writer, encoder_direction_name(dir));
    append_text(&writer, " rpm=");
    const uint32_t magnitude = (uint32_t)((rpm_x100 < 0)
                               ? -(int64_t)rpm_x100 : (int64_t)rpm_x100);
    append_unsigned(&writer, magnitude / ENCODER_CENTI_RPM_SCALE);
    append_char(&writer, '.');
    const uint32_t fraction = magnitude % ENCODER_CENTI_RPM_SCALE;
    append_char(&writer, (char)('0' + fraction / DECIMAL_BASE));
    append_char(&writer, (char)('0' + fraction % DECIMAL_BASE));
    if (missed_windows > 0U) {
        append_text(&writer, " missed=");
        append_unsigned(&writer, missed_windows);
    }
    return finish_line(&writer);
}
