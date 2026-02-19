// SPDX-FileCopyrightText: 2025-2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "a-json-sax-library/ajson_string_utils.h"
#include "a-memory-library/aml_pool.h"
#include "a-memory-library/aml_buffer.h"

#include "the-macro-library/macro_test.h"

/* ---------- Encode / Decode Helpers ---------- */

MACRO_TEST(encode_decode) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char raw[] = "Hello\t\"World\"\n";

    char *enc = ajson_encode(pool, raw, strlen(raw));
    MACRO_ASSERT_TRUE(enc != NULL);

    size_t out_len = 0;
    char *decoded = ajson_decode2(&out_len, pool, enc, strlen(enc));

    MACRO_ASSERT_EQ_SZ(out_len, strlen(raw));
    MACRO_ASSERT_TRUE(memcmp(decoded, raw, out_len) == 0);

    aml_pool_destroy(pool);
}

MACRO_TEST(decode_simple_no_escapes_zerocopy) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char enc[] = "no_escapes_here";
    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, enc, strlen(enc));

    /* Should alias the original string if no escapes are found */
    MACRO_ASSERT_TRUE(dec == enc);
    MACRO_ASSERT_EQ_SZ(out_len, strlen(enc));

    aml_pool_destroy(pool);
}

MACRO_TEST(decode_all_simple_escapes) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char enc[] = "\\n\\t\\r\\b\\f\\/\\\\\\\"";
    const char expected[8] = { '\n','\t','\r','\b','\f','/','\\','"' };

    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, enc, strlen(enc));

    MACRO_ASSERT_EQ_SZ(out_len, sizeof(expected));
    MACRO_ASSERT_TRUE(memcmp(dec, expected, sizeof(expected)) == 0);

    aml_pool_destroy(pool);
}

MACRO_TEST(encode_slash_quote_backslash) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char raw[] = { '/', '\\', '\"' };

    char *enc = ajson_encode(pool, raw, sizeof(raw));
    MACRO_ASSERT_STREQ(enc, "\\/\\\\\\\"");

    aml_pool_destroy(pool);
}

MACRO_TEST(encode_embedded_nul_and_controls) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* raw: A \0 B \n */
    char raw[] = { 'A', '\0', 'B', '\n' };

    char *enc = ajson_encode(pool, raw, sizeof(raw));
    /* Expect: A\u0000B\n  (newline becomes \n) */
    MACRO_ASSERT_STREQ(enc, "A\\u0000B\\n");

    /* No-escape case should return the same pointer */
    char ok[] = "simple";
    char *enc2 = ajson_encode(pool, ok, strlen(ok));
    MACRO_ASSERT_TRUE(enc2 == ok);

    aml_pool_destroy(pool);
}

MACRO_TEST(decode_unicode_surrogate_pair_and_invalid) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* Valid pair: U+1D11E (MUSICAL SYMBOL G CLEF) -> F0 9D 84 9E */
    char enc_pair[] = "\\uD834\\uDD1E";
    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, enc_pair, strlen(enc_pair));

    MACRO_ASSERT_EQ_SZ(out_len, 4);
    unsigned char expect[4] = {0xF0,0x9D,0x84,0x9E};
    MACRO_ASSERT_TRUE(memcmp(dec, expect, 4) == 0);

    /* Invalid lone high surrogate -> copied literally */
    char enc_bad[] = "\\uD800";
    out_len = 0;
    dec = ajson_decode2(&out_len, pool, enc_bad, strlen(enc_bad));

    MACRO_ASSERT_EQ_SZ(out_len, 6);
    MACRO_ASSERT_TRUE(memcmp(dec, "\\uD800", 6) == 0);

    aml_pool_destroy(pool);
}

MACRO_TEST(decode_invalid_unicode_escape_copied) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char enc[] = "\\u12G4";
    size_t out_len = 0;
    char *dec = ajson_decode2(&out_len, pool, enc, strlen(enc));

    MACRO_ASSERT_EQ_SZ(out_len, 6);
    MACRO_ASSERT_TRUE(memcmp(dec, "\\u12G4", 6) == 0);

    aml_pool_destroy(pool);
}

/* ---------- UTF-8 Filtering / Stripping ---------- */

MACRO_TEST(utf8_strip_invalid_inplace) {
    /* "XY" + truncated 3-byte sequence start (E2 82 missing final AC) */
    char buf[] = { 'X','Y','\xE2','\x82','Z', 0 };

    size_t new_len = ajson_strip_invalid_utf8_inplace(buf, 5);
    buf[new_len] = '\0';

    MACRO_ASSERT_EQ_SZ(new_len, 3);
    MACRO_ASSERT_STREQ(buf, "XYZ");
}

MACRO_TEST(utf8_buffer_append_valid) {
    aml_buffer_t *bh = aml_buffer_init(32);
    /* 0xC3 0x28 is invalid; output should drop 0xC3 and keep '(' */
    const char bad[] = { 'A','\xC3','\x28','B','C', 0};

    ajson_buffer_append_valid_utf8(bh, bad, 5);
    aml_buffer_appendc(bh, '\0');

    MACRO_ASSERT_STREQ(aml_buffer_data(bh), "A(BC");

    aml_buffer_destroy(bh);
}

/* -------- register & run -------- */

int main(void) {
    macro_test_case tests[64];
    size_t test_count = 0;

    MACRO_ADD(tests, encode_decode);
    MACRO_ADD(tests, decode_simple_no_escapes_zerocopy);
    MACRO_ADD(tests, decode_all_simple_escapes);
    MACRO_ADD(tests, encode_slash_quote_backslash);
    MACRO_ADD(tests, encode_embedded_nul_and_controls);
    MACRO_ADD(tests, decode_unicode_surrogate_pair_and_invalid);
    MACRO_ADD(tests, decode_invalid_unicode_escape_copied);

    MACRO_ADD(tests, utf8_strip_invalid_inplace);
    MACRO_ADD(tests, utf8_buffer_append_valid);

    macro_run_all("ajson_string_utils", tests, test_count);
    return 0;
}
