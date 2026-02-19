// SPDX-FileCopyrightText: 2025-2026 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#include "a-json-sax-library/ajson_sax.h"
#include <string.h>

/* Internal Parser Stack Constants */
#define SAX_MODE_ROOT   0
#define SAX_MODE_OBJECT 1
#define SAX_MODE_ARRAY  2

/* Max depth for the internal syntax stack (on C stack). */
#define AJSON_SAX_MAX_STACK_DEPTH 512

/* Case Macros for Tokenization */
#define AJSON_NATURAL_NUMBER_CASE                                            \
  '1' : case '2' : case '3' : case '4' : case '5' : case '6' : case '7'        \
      : case '8' : case '9'

#define AJSON_SPACE_CASE 32 : case 9 : case 13 : case 10

int ajson_sax_parse(char *p, char *ep,
                    const ajson_sax_cb_t *initial_cb,
                    aml_pool_t *pool,
                    void *ctx,
                    char **error_at) {

    /* --- Context Setup --- */
    ajson_sax_t sax;
    sax.cb = *initial_cb;
    sax.pool = pool;
    sax.top = NULL;
    sax.current_depth = 0;
    sax.anchor_depth = 0;

    /* --- Internal Syntax Stack (On Stack) --- */
    unsigned char stack[AJSON_SAX_MAX_STACK_DEPTH];
    int stack_depth = 0;
    stack[0] = SAX_MODE_ROOT;

    /* Stack Helper Macros */
    #define SYNTAX_PUSH(mode) do { \
        if (stack_depth >= AJSON_SAX_MAX_STACK_DEPTH - 1) return -1; \
        stack[++stack_depth] = (mode); \
        sax.current_depth++; \
    } while(0)

    #define SYNTAX_POP() do { \
        if (stack_depth > 0) stack_depth--; \
        sax.current_depth--; \
    } while(0)

    #define CURRENT_MODE() (stack[stack_depth])

    /* Callback Helper Macros */
    int rc = 0;

    #define SAX_ERROR { \
        if (error_at) *error_at = p; \
        return -1; \
    }

    #define CHECK_CB(x) if ((rc = (x))) { \
        if (error_at) *error_at = p; \
        return rc; \
    }

    /* --- Parsing Variables --- */
    char *sp = p;
    char ch;
    char *stringp = NULL;
    bool after_comma = false;

    if (p >= ep) SAX_ERROR;

    /* Jump into the state machine */
    goto start_value;

start_key:;
    if (p >= ep) SAX_ERROR;
    ch = *p++;
    switch (ch) {
    case '\"':
        after_comma = false;
        stringp = p;
        goto get_end_of_key;
    case AJSON_SPACE_CASE:
        goto start_key;
    case '}':
        if (after_comma) SAX_ERROR;
        if (sax.cb.on_end_object) CHECK_CB(sax.cb.on_end_object(ctx, &sax));
        SYNTAX_POP();
        goto determine_next_step;
    default:
        SAX_ERROR;
    };

get_end_of_key:;
    while (p < ep && *p != '\"') p++;
    if (p < ep) {
        if (p[-1] == '\\') {
            char *backtrack = p-1;
            while(*backtrack == '\\') backtrack--;
            if(((p-backtrack) & 1) == 0) {
                p++;
                goto get_end_of_key;
            }
        }
        *p = 0;
    }

    if (sax.cb.on_key) CHECK_CB(sax.cb.on_key(ctx, &sax, stringp, p - stringp));

    *p = '\"'; /* FIX: Restore closing quote */
    p++;
    while (p < ep && *p != ':') p++;
    p++;

start_key_object:;
    if (p >= ep) SAX_ERROR;
    ch = *p++;
    switch (ch) {
    case '\"':
        stringp = p;
        goto keyed_start_string;
    case AJSON_SPACE_CASE:
        goto start_key_object;
    case '{':
        if (sax.cb.on_start_object) CHECK_CB(sax.cb.on_start_object(ctx, &sax));
        SYNTAX_PUSH(SAX_MODE_OBJECT);
        goto start_key;
    case '[':
        if (sax.cb.on_start_array) CHECK_CB(sax.cb.on_start_array(ctx, &sax));
        SYNTAX_PUSH(SAX_MODE_ARRAY);
        goto start_value;
    case '-':
        stringp = p - 1;
        ch = *p++;
        if (ch == '0') {
            char la = *p;
            if (la == '.') { p++; goto keyed_decimal_number; }
            if (la == 'e' || la == 'E') { goto keyed_next_digit; }
            if (la >= '0' && la <= '9') { SAX_ERROR; }
            /* -0 */
            if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, "-0", 2));
            goto look_for_key;
        } else if (ch >= '1' && ch <= '9') {
            goto keyed_next_digit;
        } else {
            SAX_ERROR;
        }
    case '0': {
        stringp = p - 1;
        if (p < ep) {
            char la = *p;
            if (la == '.') { p++; goto keyed_decimal_number; }
            if (la == 'e' || la == 'E') { goto keyed_next_digit; }
            if (la >= '0' && la <= '9') { SAX_ERROR; }
        }
        if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, "0", 1));
        ch = *p;
        goto look_for_key;
    }
    case AJSON_NATURAL_NUMBER_CASE:
        stringp = p - 1;
        goto keyed_next_digit;
    case 't':
        if (p + 3 > ep) SAX_ERROR;
        if (*p++ != 'r') SAX_ERROR;
        if (*p++ != 'u') SAX_ERROR;
        if (*p++ != 'e') SAX_ERROR;
        if (sax.cb.on_bool) CHECK_CB(sax.cb.on_bool(ctx, &sax, true));
        ch = *p;
        goto look_for_key;
    case 'f':
        if (p + 4 > ep) SAX_ERROR;
        if (*p++ != 'a') SAX_ERROR;
        if (*p++ != 'l') SAX_ERROR;
        if (*p++ != 's') SAX_ERROR;
        if (*p++ != 'e') SAX_ERROR;
        if (sax.cb.on_bool) CHECK_CB(sax.cb.on_bool(ctx, &sax, false));
        ch = *p;
        goto look_for_key;
    case 'n':
        if (p + 3 > ep) SAX_ERROR;
        if (*p++ != 'u') SAX_ERROR;
        if (*p++ != 'l') SAX_ERROR;
        if (*p++ != 'l') SAX_ERROR;
        if (sax.cb.on_null) CHECK_CB(sax.cb.on_null(ctx, &sax));
        ch = *p;
        goto look_for_key;
    default:
        SAX_ERROR;
    };

keyed_start_string:;
    while (p < ep && *p != '\"') p++;
    if (p >= ep) SAX_ERROR;
    if (p[-1] == '\\') {
        char *backtrack = p-1;
        while(*backtrack == '\\') backtrack--;
        if(((p-backtrack) & 1) == 0) {
            p++;
            goto keyed_start_string;
        }
    }
    *p = 0;
    if (sax.cb.on_string) CHECK_CB(sax.cb.on_string(ctx, &sax, stringp, p - stringp));

    *p = '\"'; /* FIX: Restore closing quote */
    p++;
    ch = *p;

look_for_key:;
    switch (ch) {
    case ',':
        if (p >= ep) SAX_ERROR;
        p++;
        after_comma = true;
        goto start_key;
    case '}':
        if (after_comma) SAX_ERROR;
        if (sax.cb.on_end_object) CHECK_CB(sax.cb.on_end_object(ctx, &sax));
        SYNTAX_POP();
        p++;
        goto determine_next_step;
    case AJSON_SPACE_CASE:
        p++;
        ch = *p;
        goto look_for_key;
    default:
        SAX_ERROR;
    };

keyed_next_digit:;
    ch = *p++;
    while ((ch >= '0' && ch <= '9')) ch = *p++;

    if (ch == '.') goto keyed_decimal_number;
    if (ch == 'e' || ch == 'E') {
        ch = *p++;
        if (ch == '+' || ch == '-') ch = *p++;
        if (ch < '0' || ch > '9') SAX_ERROR;
        while (ch >= '0' && ch <= '9') ch = *p++;
    }
    p--; *p = 0;
    if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, stringp, p - stringp));
    *p = ch; /* Restore delimiter */
    goto look_for_key;

keyed_decimal_number:;
    ch = *p++;
    if (ch < '0' || ch > '9') SAX_ERROR;
    while (ch >= '0' && ch <= '9') ch = *p++;

    if (ch == 'e' || ch == 'E') {
        ch = *p++;
        if (ch == '+' || ch == '-') ch = *p++;
        if (ch < '0' || ch > '9') SAX_ERROR;
        while (ch >= '0' && ch <= '9') ch = *p++;
    }
    p--; *p = 0;
    if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, stringp, p - stringp));
    *p = ch; /* Restore delimiter */
    goto look_for_key;

start_value:;
    if (p >= ep) SAX_ERROR;
    ch = *p++;
    switch (ch) {
    case '\"':
        after_comma = false;
        stringp = p;
        goto start_string;
    case AJSON_SPACE_CASE:
        goto start_value;
    case '{':
        after_comma = false;
        if (sax.cb.on_start_object) CHECK_CB(sax.cb.on_start_object(ctx, &sax));
        SYNTAX_PUSH(SAX_MODE_OBJECT);
        goto start_key;
    case '[':
        after_comma = false;
        if (sax.cb.on_start_array) CHECK_CB(sax.cb.on_start_array(ctx, &sax));
        SYNTAX_PUSH(SAX_MODE_ARRAY);
        goto start_value;
    case ']':
        if (after_comma) SAX_ERROR;
        if (sax.cb.on_end_array) CHECK_CB(sax.cb.on_end_array(ctx, &sax));
        SYNTAX_POP();
        goto determine_next_step;
    case '-':
        after_comma = false;
        stringp = p - 1;
        ch = *p++;
        if (ch == '0') {
            char la = *p;
            if (la == '.') { p++; goto decimal_number; }
            if (la == 'e' || la == 'E') { goto next_digit; }
            if (la >= '0' && la <= '9') SAX_ERROR;
            /* -0 */
            if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, "-0", 2));
            ch = *p;
            goto add_string;
        } else if (ch >= '1' && ch <= '9') {
            goto next_digit;
        } else {
            SAX_ERROR;
        }
    case '0': {
        after_comma = false;
        stringp = p - 1;
        if (p < ep) {
            char la = *p;
            if (la == '.') { p++; goto decimal_number; }
            if (la == 'e' || la == 'E') { goto next_digit; }
            if (la >= '0' && la <= '9') { SAX_ERROR; }
        }
        if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, "0", 1));
        ch = *p;
        goto add_string;
    }
    case AJSON_NATURAL_NUMBER_CASE:
        after_comma = false;
        stringp = p - 1;
        goto next_digit;
    case 't':
        after_comma = false;
        if (p + 3 > ep) SAX_ERROR;
        if (*p++ != 'r') SAX_ERROR;
        if (*p++ != 'u') SAX_ERROR;
        if (*p++ != 'e') SAX_ERROR;
        if (sax.cb.on_bool) CHECK_CB(sax.cb.on_bool(ctx, &sax, true));
        ch = *p;
        goto add_string;
    case 'f':
        after_comma = false;
        if (p + 4 > ep) SAX_ERROR;
        if (*p++ != 'a') SAX_ERROR;
        if (*p++ != 'l') SAX_ERROR;
        if (*p++ != 's') SAX_ERROR;
        if (*p++ != 'e') SAX_ERROR;
        if (sax.cb.on_bool) CHECK_CB(sax.cb.on_bool(ctx, &sax, false));
        ch = *p;
        goto add_string;
    case 'n':
        after_comma = false;
        if (p + 3 > ep) SAX_ERROR;
        if (*p++ != 'u') SAX_ERROR;
        if (*p++ != 'l') SAX_ERROR;
        if (*p++ != 'l') SAX_ERROR;
        if (sax.cb.on_null) CHECK_CB(sax.cb.on_null(ctx, &sax));
        ch = *p;
        goto add_string;
    default:
        SAX_ERROR;
    };

start_string:;
    while (p < ep && *p != '\"') p++;
    if (p >= ep) SAX_ERROR;
    if (p[-1] == '\\') {
        char *backtrack = p-1;
        while(*backtrack == '\\') backtrack--;
        if(((p-backtrack) & 1) == 0) {
            p++;
            goto start_string;
        }
    }
    *p = 0;
    if (sax.cb.on_string) CHECK_CB(sax.cb.on_string(ctx, &sax, stringp, p - stringp));

    *p = '\"'; /* FIX: Restore closing quote */
    p++;
    ch = *p;

add_string:;
    goto look_for_next_object;

look_for_next_object:;
    switch (ch) {
    case ',':
        if (p >= ep) SAX_ERROR;
        p++;
        after_comma = true;
        goto start_value;
    case ']':
        if (after_comma) SAX_ERROR;
        if (sax.cb.on_end_array) CHECK_CB(sax.cb.on_end_array(ctx, &sax));
        SYNTAX_POP();
        p++;
        goto determine_next_step;
    case AJSON_SPACE_CASE:
        p++;
        if (p >= ep) {
            if (stack_depth == 0) return 0;
            SAX_ERROR;
        }
        ch = *p;
        goto look_for_next_object;
    case 0:
        if (stack_depth == 0) return 0;
        SAX_ERROR;
    default:
        SAX_ERROR;
    };

next_digit:;
    ch = *p++;
    while ((ch >= '0' && ch <= '9')) ch = *p++;

    if (ch == '.') goto decimal_number;
    if (ch == 'e' || ch == 'E') {
        ch = *p++;
        if (ch == '+' || ch == '-') ch = *p++;
        if (ch < '0' || ch > '9') SAX_ERROR;
        while (ch >= '0' && ch <= '9') ch = *p++;
    }
    p--; *p = 0;
    if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, stringp, p - stringp));
    *p = ch; /* Restore delimiter */
    goto add_string;

decimal_number:;
    ch = *p++;
    if (ch < '0' || ch > '9') SAX_ERROR;
    while (ch >= '0' && ch <= '9') ch = *p++;

    if (ch == 'e' || ch == 'E') {
        ch = *p++;
        if (ch == '+' || ch == '-') ch = *p++;
        if (ch < '0' || ch > '9') SAX_ERROR;
        while (ch >= '0' && ch <= '9') ch = *p++;
    }
    p--; *p = 0;
    if (sax.cb.on_number) CHECK_CB(sax.cb.on_number(ctx, &sax, stringp, p - stringp));
    *p = ch; /* Restore delimiter */
    goto add_string;

determine_next_step:
    if (stack_depth == 0) return 0;

    if (p >= ep) return 0;
    ch = *p;

    if (CURRENT_MODE() == SAX_MODE_OBJECT) {
        goto look_for_key;
    } else {
        goto look_for_next_object;
    }

    SAX_ERROR;
}
