// SPDX-FileCopyrightText: 2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#ifndef _AJSON_STRING_UTILS_H
#define _AJSON_STRING_UTILS_H

#include "a-memory-library/aml_pool.h"
#include "a-memory-library/aml_buffer.h"

#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================
 * Encode / decode strings
 * ======================== */

/** Decode JSON escape sequences in [s, s+length) into UTF-8.
 * May return 's' unchanged if no escapes are present (aliasing!).
 * Otherwise returns a pool-allocated buffer. */
char *ajson_decode(aml_pool_t *pool, char *s, size_t length);

/** Decode like ajson_decode, but also returns decoded length via *rlen.
 * May alias input if nothing to decode. */
char *ajson_decode2(size_t *rlen, aml_pool_t *pool, char *s, size_t length);

/** Escape JSON specials/control bytes in [s, s+length).
 * May return 's' unchanged if nothing needs escaping (aliasing!),
 * otherwise returns a pool-allocated buffer. */
char *ajson_encode(aml_pool_t *pool, char *s, size_t length);

/* ========================
 * UTF-8 Validation / Copy
 * ======================== */

/** Write only valid UTF-8 sequences from src[0..len) to FILE* out. */
void ajson_file_write_valid_utf8(FILE *out, const char *src, size_t len);

/** Copies bytes from 'src' (length 'len') to 'dest', skipping any invalid UTF-8.
 * Returns a pointer to the position after the last valid byte written. */
char *ajson_copy_valid_utf8(char *dest, const char *src, size_t len);

/** Appends the contents of 'src' to the aml_buffer, skipping invalid UTF-8. */
void ajson_buffer_append_valid_utf8(aml_buffer_t *bh, const char *src, size_t len);

/** Modifies the given buffer 'str' so it contains only valid UTF-8 bytes.
 * Returns the new length (number of valid bytes). */
size_t ajson_strip_invalid_utf8_inplace(char *str, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* _AJSON_STRING_UTILS_H */
