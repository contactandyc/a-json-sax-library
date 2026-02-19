// SPDX-FileCopyrightText: 2025 Andy Curtis <contactandyc@gmail.com>
// SPDX-License-Identifier: Apache-2.0

#ifndef _ajson_sax_H
#define _ajson_sax_H

#include "a-memory-library/aml_pool.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct ajson_sax_s ajson_sax_t;

/* * The Virtual Method Table (VTable) for handlers.
 * Define these as 'static const' structs in your code to avoid
 * reconstruction overhead.
 */
typedef struct {
  int (*on_null)(void *ctx, ajson_sax_t *sax);
  int (*on_bool)(void *ctx, ajson_sax_t *sax, bool val);
  int (*on_number)(void *ctx, ajson_sax_t *sax, const char *val, size_t len);
  int (*on_string)(void *ctx, ajson_sax_t *sax, const char *val, size_t len);

  int (*on_key)(void *ctx, ajson_sax_t *sax, const char *key, size_t len);

  int (*on_start_object)(void *ctx, ajson_sax_t *sax);
  int (*on_end_object)(void *ctx, ajson_sax_t *sax);

  int (*on_start_array)(void *ctx, ajson_sax_t *sax);
  int (*on_end_array)(void *ctx, ajson_sax_t *sax);
} ajson_sax_cb_t;

/* Internal stack node for saving handler state */
typedef struct sax_handler_node_s {
    ajson_sax_cb_t cb;
    int anchor_depth;
    struct sax_handler_node_s *next;
} sax_handler_node_t;

/* * The SAX Context.
 * Passed to every callback. Manages the stack of handlers.
 */
struct ajson_sax_s {
    ajson_sax_cb_t cb;       /* The ACTIVE handlers */
    aml_pool_t *pool;        /* Memory source for the stack */
    sax_handler_node_t *top; /* The stack head */
    int current_depth;       /* Tracked by the parser */
    int anchor_depth;        /* The depth where current 'cb' took control */
};

/* * Main Parse Function.
 * - Modifies 'p' in-place (adds NUL terminators).
 * - Uses 'pool' only for the handler stack (not for nodes).
 * - 'initial_cb' sets the root handlers.
 */
int ajson_sax_parse(char *p, char *ep,
                    const ajson_sax_cb_t *initial_cb,
                    aml_pool_t *pool,
                    void *ctx,
                    char **error_at);

/* * Stack Operations
 * Use these inside your callbacks (e.g., inside on_start_object).
 */

/* Push a new set of handlers. They become active immediately. */
static inline void ajson_sax_push(ajson_sax_t *sax, const ajson_sax_cb_t *new_cb) {
    sax_handler_node_t *node = (sax_handler_node_t *)aml_pool_alloc(sax->pool, sizeof(sax_handler_node_t));

    /* Save current state */
    node->cb = sax->cb;
    node->anchor_depth = sax->anchor_depth;
    node->next = sax->top;
    sax->top = node;

    /* Load new state (if provided) */
    if (new_cb) {
        sax->cb = *new_cb;
        /* FIX: The handler is being pushed to handle the *contents* of the new object/array.
           Those contents exist at (current_depth + 1). */
        sax->anchor_depth = sax->current_depth + 1;
    }
}

/* Pop the current handlers and restore the previous state. */
static inline void ajson_sax_pop(ajson_sax_t *sax) {
    if (!sax->top) return;

    sax->cb = sax->top->cb;
    sax->anchor_depth = sax->top->anchor_depth;
    sax->top = sax->top->next;
}

/* * Helper: Only pop if we are at the depth where we started.
 * Useful for "pass-through" handlers (like Config inside Root).
 */
static inline bool ajson_sax_try_pop(ajson_sax_t *sax) {
    if (sax->current_depth == sax->anchor_depth) {
        ajson_sax_pop(sax);
        return true;
    }
    return false;
}

#ifdef __cplusplus
}
#endif

#endif /* _ajson_sax_H */
