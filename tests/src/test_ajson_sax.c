// SPDX-FileCopyrightText: 2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

// tests/test_sax.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "a-json-sax-library/ajson_sax.h"
#include "a-memory-library/aml_pool.h"
#include "a-memory-library/aml_buffer.h"

#include "the-macro-library/macro_test.h"

/* --- Contexts for tracking test state --- */

typedef struct {
    int obj_start_count;
    int obj_end_count;
    int arr_start_count;
    int arr_end_count;
    int key_count;
    int str_count;
    int num_count;
    int bool_count;
    int null_count;
    char last_key[64];
    char last_val[64];
} stats_ctx_t;

/* Reset stats helper */
static void reset_stats(stats_ctx_t *s) {
    memset(s, 0, sizeof(stats_ctx_t));
}

/* --- Generic Callbacks for Stats --- */

static int stats_on_start_obj(void *ctx, ajson_sax_t *sax) { (void)sax; ((stats_ctx_t*)ctx)->obj_start_count++; return 0; }
static int stats_on_end_obj(void *ctx, ajson_sax_t *sax) { (void)sax; ((stats_ctx_t*)ctx)->obj_end_count++; return 0; }
static int stats_on_start_arr(void *ctx, ajson_sax_t *sax) { (void)sax; ((stats_ctx_t*)ctx)->arr_start_count++; return 0; }
static int stats_on_end_arr(void *ctx, ajson_sax_t *sax) { (void)sax; ((stats_ctx_t*)ctx)->arr_end_count++; return 0; }
static int stats_on_null(void *ctx, ajson_sax_t *sax) { (void)sax; ((stats_ctx_t*)ctx)->null_count++; return 0; }
static int stats_on_bool(void *ctx, ajson_sax_t *sax, bool v) { (void)sax; (void)v; ((stats_ctx_t*)ctx)->bool_count++; return 0; }

static int stats_on_key(void *ctx, ajson_sax_t *sax, const char *key, size_t len) {
    (void)sax;
    stats_ctx_t *s = (stats_ctx_t*)ctx;
    s->key_count++;
    if(len < sizeof(s->last_key)) {
        memcpy(s->last_key, key, len);
        s->last_key[len] = 0;
    }
    return 0;
}

static int stats_on_str(void *ctx, ajson_sax_t *sax, const char *val, size_t len) {
    (void)sax;
    stats_ctx_t *s = (stats_ctx_t*)ctx;
    s->str_count++;
    if(len < sizeof(s->last_val)) {
        memcpy(s->last_val, val, len);
        s->last_val[len] = 0;
    }
    return 0;
}

static int stats_on_num(void *ctx, ajson_sax_t *sax, const char *val, size_t len) {
    (void)sax;
    stats_ctx_t *s = (stats_ctx_t*)ctx;
    s->num_count++;
    if(len < sizeof(s->last_val)) {
        memcpy(s->last_val, val, len);
        s->last_val[len] = 0;
    }
    return 0;
}

static const ajson_sax_cb_t stats_handlers = {
    .on_start_object = stats_on_start_obj, .on_end_object = stats_on_end_obj,
    .on_start_array = stats_on_start_arr, .on_end_array = stats_on_end_arr,
    .on_key = stats_on_key,
    .on_string = stats_on_str,
    .on_number = stats_on_num,
    .on_bool = stats_on_bool,
    .on_null = stats_on_null
};

/* ---------- 1) Basic Smoke Tests ---------- */

MACRO_TEST(sax_smoke_primitives) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"s\":\"hello\", \"n\": 123, \"b\": true, \"z\": null}";

    stats_ctx_t st; reset_stats(&st);

    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(st.obj_start_count, 1);
    MACRO_ASSERT_EQ_INT(st.obj_end_count, 1);
    MACRO_ASSERT_EQ_INT(st.key_count, 4);
    MACRO_ASSERT_EQ_INT(st.str_count, 1);
    MACRO_ASSERT_EQ_INT(st.num_count, 1);
    MACRO_ASSERT_EQ_INT(st.bool_count, 1);
    MACRO_ASSERT_EQ_INT(st.null_count, 1);

    MACRO_ASSERT_STREQ(st.last_key, "z"); /* Insert order */

    aml_pool_destroy(pool);
}

MACRO_TEST(sax_nested_structures) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"arr\": [1, {\"k\":\"v\"}], \"o\": {}}";

    stats_ctx_t st; reset_stats(&st);
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(st.obj_start_count, 3); /* root, item in arr, "o" */
    MACRO_ASSERT_EQ_INT(st.arr_start_count, 1);
    MACRO_ASSERT_EQ_INT(st.key_count, 3);       /* arr, k, o */

    aml_pool_destroy(pool);
}

/* ---------- 2) Stack / Push / Pop Logic ---------- */

/* Unified Context for Stack Test */
typedef struct {
    char current_key[32];
    int user_num;
    int root_num;
} unified_ctx_t;

static int u_user_num(void *ctx, ajson_sax_t *sax, const char *v, size_t l) {
    (void)sax; (void)l;
    ((unified_ctx_t*)ctx)->user_num = atoi(v);
    return 0;
}
static int u_user_end(void *ctx, ajson_sax_t *sax) {
    (void)ctx;
    ajson_sax_try_pop(sax);
    return 0;
}
static const ajson_sax_cb_t u_user_handlers = { .on_number = u_user_num, .on_end_object = u_user_end };

static int u_root_key(void *ctx, ajson_sax_t *sax, const char *k, size_t l) {
    (void)sax; (void)l;
    strcpy(((unified_ctx_t*)ctx)->current_key, k);
    return 0;
}
static int u_root_start(void *ctx, ajson_sax_t *sax) {
    if(!strcmp(((unified_ctx_t*)ctx)->current_key, "special")) {
        ajson_sax_push(sax, &u_user_handlers);
    }
    return 0;
}
static int u_root_num(void *ctx, ajson_sax_t *sax, const char *v, size_t l) {
    (void)sax; (void)l;
    ((unified_ctx_t*)ctx)->root_num = atoi(v);
    return 0;
}
static const ajson_sax_cb_t u_root_handlers = {
    .on_key = u_root_key, .on_start_object = u_root_start, .on_number = u_root_num
};

MACRO_TEST(sax_stack_unified_logic) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* "special" triggers push, "normal" does not */
    char json[] = "{\"special\": {\"v\": 100}, \"normal\": {\"v\": 200}}";

    unified_ctx_t c = {0};
    int rc = ajson_sax_parse(json, json + strlen(json), &u_root_handlers, pool, &c, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(c.user_num, 100);
    MACRO_ASSERT_EQ_INT(c.root_num, 200); /* Root handled the second object */

    aml_pool_destroy(pool);
}

/* ---------- 3) Anchor Depth Correctness ---------- */

static int anchor_end(void *ctx, ajson_sax_t *sax) {
    if (ajson_sax_try_pop(sax)) {
        *((int*)ctx) += 1;
    }
    return 0;
}
static const ajson_sax_cb_t anchor_h = { .on_end_object = anchor_end };

static int anchor_start_root(void *ctx, ajson_sax_t *sax) {
    (void)ctx;
    /* Push the anchor handler immediately on first object */
    ajson_sax_push(sax, &anchor_h);
    return 0;
}
static const ajson_sax_cb_t anchor_root = { .on_start_object = anchor_start_root };

MACRO_TEST(sax_anchor_correctness) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"a\": { \"b\": {} }}";
    int pop_count = 0;

    int rc = ajson_sax_parse(json, json+strlen(json), &anchor_root, pool, &pop_count, NULL);
    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(pop_count, 1); /* Should only pop once, at the end */

    aml_pool_destroy(pool);
}

/* ---------- 4) Abort and Error ---------- */

static int abort_on_key(void *ctx, ajson_sax_t *sax, const char *k, size_t l) {
    (void)ctx; (void)sax; (void)l;
    if (!strcmp(k, "abort")) return 42;
    return 0;
}
static const ajson_sax_cb_t abort_h = { .on_key = abort_on_key };

MACRO_TEST(sax_abort_code) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"ok\": 1, \"abort\": 0, \"ignored\": 1}";
    int rc = ajson_sax_parse(json, json+strlen(json), &abort_h, pool, NULL, NULL);
    MACRO_ASSERT_EQ_INT(rc, 42);
    aml_pool_destroy(pool);
}

MACRO_TEST(sax_syntax_error) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"missing_colon\" 1}";

    /* FIX: Pass a valid struct, even if we don't check the stats,
       because stats_handlers writes to it. */
    stats_ctx_t st;

    int rc = ajson_sax_parse(json, json+strlen(json), &stats_handlers, pool, &st, NULL);
    MACRO_ASSERT_EQ_INT(rc, -1);
    aml_pool_destroy(pool);
}

/* ---------- 5) Limits ---------- */

MACRO_TEST(sax_stack_overflow) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    aml_buffer_t *bh = aml_buffer_init(1024);

    /* 512 is max depth. Create 515. */
    for(int i=0; i<515; i++) aml_buffer_appendc(bh, '[');
    for(int i=0; i<515; i++) aml_buffer_appendc(bh, ']');

    /* FIX: Must pass a valid context because stats_handlers writes to it */
    stats_ctx_t st;

    int rc = ajson_sax_parse(aml_buffer_data(bh),
                             aml_buffer_data(bh) + aml_buffer_length(bh),
                             &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, -1); /* Should fail */

    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}

/* ---------- 6) Values ---------- */

MACRO_TEST(sax_number_formats) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "[ -0, 0, 1.25e+2 ]";
    stats_ctx_t st; reset_stats(&st);

    ajson_sax_parse(json, json+strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(st.num_count, 3);
    /* Last one should be 1.25e+2 */
    MACRO_ASSERT_STREQ(st.last_val, "1.25e+2");

    aml_pool_destroy(pool);
}

MACRO_TEST(sax_empty_structures) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"a\": [], \"b\": {}, \"c\": [{}], \"d\": {\"e\": []}}";

    stats_ctx_t st; reset_stats(&st);
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(st.arr_start_count, 3); /* a, c, e */
    MACRO_ASSERT_EQ_INT(st.arr_end_count, 3);
    MACRO_ASSERT_EQ_INT(st.obj_start_count, 4); /* root, b, item in c, d */
    MACRO_ASSERT_EQ_INT(st.obj_end_count, 4);

    aml_pool_destroy(pool);
}

MACRO_TEST(sax_trailing_comma_fail) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"a\": 1, }";

    stats_ctx_t st;
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, -1);

    char json2[] = "[1, 2, ]";
    rc = ajson_sax_parse(json2, json2 + strlen(json2), &stats_handlers, pool, &st, NULL);
    MACRO_ASSERT_EQ_INT(rc, -1);

    aml_pool_destroy(pool);
}

MACRO_TEST(sax_mixed_nested_depth) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* Root(Obj) -> Arr -> Obj -> Arr -> Int */
    char json[] = "{\"l1\": [ { \"l3\": [ 42 ] } ] }";

    stats_ctx_t st; reset_stats(&st);
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(st.num_count, 1);
    MACRO_ASSERT_EQ_INT(st.arr_start_count, 2);
    MACRO_ASSERT_EQ_INT(st.obj_start_count, 2);

    aml_pool_destroy(pool);
}

MACRO_TEST(sax_root_array) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "[1, true, null, \"end\"]";

    stats_ctx_t st; reset_stats(&st);
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(st.arr_start_count, 1);
    MACRO_ASSERT_EQ_INT(st.arr_end_count, 1);
    MACRO_ASSERT_EQ_INT(st.num_count, 1);
    MACRO_ASSERT_EQ_INT(st.bool_count, 1);
    MACRO_ASSERT_EQ_INT(st.null_count, 1);
    MACRO_ASSERT_EQ_INT(st.str_count, 1);

    aml_pool_destroy(pool);
}


/* Note: reckless_h is unused in the unit test below, but defined for completeness if needed later.
   To avoid "unused variable" warnings, we can wrap it or just use the unit test logic below. */
MACRO_TEST(sax_manual_pop_safety) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* Direct unit test of the inline ajson_sax_try_pop logic */
    ajson_sax_t sax = {0};
    sax.pool = pool;

    /* Mock stack node so pop doesn't crash if it succeeds */
    sax_handler_node_t mock_node = {0};
    sax.top = &mock_node;

    /* 1. Mismatch: Current Depth (5) != Anchor Depth (2) */
    sax.current_depth = 5;
    sax.anchor_depth = 2;
    /* Should return false and NOT pop */
    MACRO_ASSERT_FALSE(ajson_sax_try_pop(&sax));
    MACRO_ASSERT_TRUE(sax.top == &mock_node);

    /* 2. Match: Current Depth (2) == Anchor Depth (2) */
    sax.current_depth = 2;
    /* Should return true and pop */
    MACRO_ASSERT_TRUE(ajson_sax_try_pop(&sax));
    MACRO_ASSERT_TRUE(sax.top == NULL); /* mock_node.next was NULL */

    aml_pool_destroy(pool);
}

/* ---------- 8) String Escaping and Raw Slices ---------- */

MACRO_TEST(sax_string_escapes) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* Input: "val\"ue" (encoded quote) and "line\nbreak" (encoded newline) */
    char json[] = "{\"k\": \"val\\\"ue\", \"k2\": \"line\\nbreak\"}";

    stats_ctx_t st; reset_stats(&st);
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(st.str_count, 2);

    /* Verify the parser correctly skipped the escaped quote and captured the full raw string */
    /* Note: SAX parser returns the RAW JSON slice (null-terminated), it does NOT decode escapes. */
    MACRO_ASSERT_STREQ(st.last_val, "line\\nbreak");

    aml_pool_destroy(pool);
}

/* ---------- 9) Whitespace Torture ---------- */

MACRO_TEST(sax_whitespace_torture) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* Lots of tabs, newlines, and spaces in valid positions */
    char json[] = " \n\t { \n \"a\" \t : \n [ \t 1 \n , \r 2 \t ] \n } \t ";

    stats_ctx_t st; reset_stats(&st);
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_EQ_INT(st.obj_start_count, 1);
    MACRO_ASSERT_EQ_INT(st.arr_start_count, 1);
    MACRO_ASSERT_EQ_INT(st.num_count, 2);

    aml_pool_destroy(pool);
}

/* ---------- 10) Invalid Literals (Case Sensitivity) ---------- */

MACRO_TEST(sax_invalid_literals) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* We must provide a valid context because the parser will
       trigger on_start_object BEFORE it fails on the invalid value. */
    stats_ctx_t st;

    /* JSON is case-sensitive. 'True' is invalid. */
    char json1[] = "{\"a\": True}";
    int rc = ajson_sax_parse(json1, json1 + strlen(json1), &stats_handlers, pool, &st, NULL);
    MACRO_ASSERT_EQ_INT(rc, -1);

    /* 'NULL' is invalid */
    char json2[] = "{\"a\": NULL}";
    rc = ajson_sax_parse(json2, json2 + strlen(json2), &stats_handlers, pool, &st, NULL);
    MACRO_ASSERT_EQ_INT(rc, -1);

    aml_pool_destroy(pool);
}

MACRO_TEST(sax_root_scalars) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* Valid JSON documents */
    char j1[] = "123";
    char j2[] = "\"hello\"";
    char j3[] = "true";
    char j4[] = "null";

    stats_ctx_t st;

    /* These should all PASS (rc=0) */
    MACRO_ASSERT_EQ_INT(ajson_sax_parse(j1, j1+strlen(j1), &stats_handlers, pool, &st, NULL), 0);
    MACRO_ASSERT_EQ_INT(ajson_sax_parse(j2, j2+strlen(j2), &stats_handlers, pool, &st, NULL), 0);
    MACRO_ASSERT_EQ_INT(ajson_sax_parse(j3, j3+strlen(j3), &stats_handlers, pool, &st, NULL), 0);
    MACRO_ASSERT_EQ_INT(ajson_sax_parse(j4, j4+strlen(j4), &stats_handlers, pool, &st, NULL), 0);

    /* Whitespace after root scalar */
    char j5[] = "123   ";
    MACRO_ASSERT_EQ_INT(ajson_sax_parse(j5, j5+strlen(j5), &stats_handlers, pool, &st, NULL), 0);

    aml_pool_destroy(pool);
}

/* ---------- 11) Null Handler Safety ---------- */
MACRO_TEST(sax_null_handlers_safety) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    char json[] = "{\"a\": [1, true, null]}";

    /* Pass a completely empty callback struct.
       The parser should run through the JSON, validate it,
       and do nothing (no crashes). */
    ajson_sax_cb_t empty_cb = {0};

    int rc = ajson_sax_parse(json, json + strlen(json), &empty_cb, pool, NULL, NULL);
    MACRO_ASSERT_EQ_INT(rc, 0);

    aml_pool_destroy(pool);
}

/* ---------- 12) Truncated Input Torture ---------- */
MACRO_TEST(sax_truncated_torture) {
    aml_pool_t *pool = aml_pool_init(1 << 12);

    /* A list of valid JSON prefixes that should fail gracefully (-1), NOT crash */
    const char *bad_inputs[] = {
        "{",            /* Unclosed object */
        "{\"a\"",       /* Unclosed key */
        "{\"a\":",      /* Missing value */
        "{\"a\":1",     /* Missing closing brace */
        "[",            /* Unclosed array */
        "[1",           /* Missing closing bracket */
        "[1,",          /* Trailing comma / missing value */
        "\"unclosed",   /* Unclosed root string */
        "tru",          /* Truncated true */
        "fals",         /* Truncated false */
        "nul"           /* Truncated null */
    };

    ajson_sax_cb_t empty_cb = {0};

    for (size_t i = 0; i < sizeof(bad_inputs)/sizeof(bad_inputs[0]); ++i) {
        char *buf = strdup(bad_inputs[i]); /* Copy because parser modifies in-place */
        int rc = ajson_sax_parse(buf, buf + strlen(buf), &empty_cb, pool, NULL, NULL);

        /* Must return error */
        if (rc != -1) {
            printf("Failed to reject truncated input: '%s'\n", bad_inputs[i]);
            MACRO_ASSERT_EQ_INT(rc, -1);
        }

        free(buf);
    }

    aml_pool_destroy(pool);
}

/* ---------- 13) Exact Depth Limit Boundary ---------- */
MACRO_TEST(sax_depth_boundary) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    aml_buffer_t *bh = aml_buffer_init(1024);

    /* Max depth is 512.
       Depth 0: Root.
       Depth 1: [
       ...
       Depth 511: [  <-- Last valid push
       Depth 512: [  <-- Should fail
    */

    /* Case 1: 511 nested arrays (Total depth 512 including root). Should PASS. */
    /* Root is depth 0. We push 511 arrays. stack[1]...stack[511]. */
    int limit = 511;
    aml_buffer_clear(bh);
    for(int i=0; i<limit; i++) aml_buffer_appendc(bh, '[');
    for(int i=0; i<limit; i++) aml_buffer_appendc(bh, ']');

    ajson_sax_cb_t empty_cb = {0};
    int rc = ajson_sax_parse(aml_buffer_data(bh),
                             aml_buffer_data(bh) + aml_buffer_length(bh),
                             &empty_cb, pool, NULL, NULL);
    MACRO_ASSERT_EQ_INT(rc, 0);

    /* Case 2: 512 nested arrays. Should FAIL. */
    limit = 512;
    aml_buffer_clear(bh);
    for(int i=0; i<limit; i++) aml_buffer_appendc(bh, '[');
    for(int i=0; i<limit; i++) aml_buffer_appendc(bh, ']');

    rc = ajson_sax_parse(aml_buffer_data(bh),
                         aml_buffer_data(bh) + aml_buffer_length(bh),
                         &empty_cb, pool, NULL, NULL);
    MACRO_ASSERT_EQ_INT(rc, -1);

    aml_buffer_destroy(bh);
    aml_pool_destroy(pool);
}

/* ---------- 14) Unicode Key Integrity ---------- */
MACRO_TEST(sax_unicode_raw_keys) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* Input: {"\u00E9": 1} where \u00E9 is 'é' */
    /* The SAX parser should pass the key string raw, without decoding/unescaping. */
    char json[] = "{\"\\u00E9\": 1}";

    stats_ctx_t st; reset_stats(&st);
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, NULL);

    MACRO_ASSERT_EQ_INT(rc, 0);
    MACRO_ASSERT_STREQ(st.last_key, "\\u00E9");

    aml_pool_destroy(pool);
}

MACRO_TEST(sax_error_reporting) {
    aml_pool_t *pool = aml_pool_init(1 << 12);
    /* Error at index 14 (the 'x' in 12x) */
    char json[] = "{\"a\": [100, 12x, 300]}";

    /* FIX: Must provide valid context for stats_handlers */
    stats_ctx_t st;

    char *err_loc = NULL;
    int rc = ajson_sax_parse(json, json + strlen(json), &stats_handlers, pool, &st, &err_loc);

    MACRO_ASSERT_EQ_INT(rc, -1);
    MACRO_ASSERT_TRUE(err_loc != NULL);

    /* Calculate offset */
    long offset = err_loc - json;
    MACRO_ASSERT_EQ_INT((int)offset, 14);
    MACRO_ASSERT_EQ_INT(*err_loc, 'x');

    aml_pool_destroy(pool);
}

/* ---------- Register ---------- */

int main(void) {
    macro_test_case tests[64];
    size_t test_count = 0;

    MACRO_ADD(tests, sax_smoke_primitives);
    MACRO_ADD(tests, sax_nested_structures);
    MACRO_ADD(tests, sax_stack_unified_logic);
    MACRO_ADD(tests, sax_anchor_correctness);
    MACRO_ADD(tests, sax_abort_code);
    MACRO_ADD(tests, sax_syntax_error);
    MACRO_ADD(tests, sax_stack_overflow);
    MACRO_ADD(tests, sax_number_formats);

    MACRO_ADD(tests, sax_empty_structures);
    MACRO_ADD(tests, sax_trailing_comma_fail);
    MACRO_ADD(tests, sax_mixed_nested_depth);
    MACRO_ADD(tests, sax_root_array);
    MACRO_ADD(tests, sax_manual_pop_safety);

    MACRO_ADD(tests, sax_string_escapes);
    MACRO_ADD(tests, sax_whitespace_torture);
    MACRO_ADD(tests, sax_invalid_literals);

    MACRO_ADD(tests, sax_root_scalars);

    MACRO_ADD(tests, sax_null_handlers_safety);
    MACRO_ADD(tests, sax_truncated_torture);
    MACRO_ADD(tests, sax_depth_boundary);
    MACRO_ADD(tests, sax_unicode_raw_keys);

    MACRO_ADD(tests, sax_error_reporting);

    macro_run_all("ajson_sax", tests, test_count);
    return 0;
}
