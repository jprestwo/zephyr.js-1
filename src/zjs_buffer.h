// Copyright (c) 2016-2017, Intel Corporation.

#ifndef __zjs_buffer_h__
#define __zjs_buffer_h__

#include "jerryscript.h"

#ifdef CONFIG_NETWORKING
#include <net/net_pkt.h>
#endif

// ZJS includes
#include "zjs_common.h"

/** Initialize the buffer module, or reinitialize after cleanup */
void zjs_buffer_init();

/** Release resources held by the buffer module */
void zjs_buffer_cleanup();

// FIXME: We should make this private and have accessor methods
typedef struct zjs_buffer {
#ifdef CONFIG_NETWORKING
    struct net_buf *net_buf;
#endif
    u8_t *buffer;
    u32_t bufsize;
} zjs_buffer_t;

/**
 * Test whether the given value is a Buffer object
 *
 * @param value  A JerryScript value
 *
 * @return  true if the value is a Buffer object; false otherwise
 */
bool zjs_value_is_buffer(const jerry_value_t value);

/**
 * Returns the buffer handle associated with obj, if found
 *
 * @param obj  A JerryScript object value
 *
 * @return  A pointer to the handle structure for this Buffer object, or NULL.
 */
zjs_buffer_t *zjs_buffer_find(const jerry_value_t obj);

/**
 * Create a new Buffer object
 *
 * @param size     Buffer size in bytes
 * @param ret_buf  Output pointer to receive new buffer handle, or NULL
 *
 * @return  New JS Buffer or Error object, and sets *ret_buf to C handle or
 *            NULL, if given
 */
jerry_value_t zjs_buffer_create(u32_t size, zjs_buffer_t **ret_buf);

#ifdef CONFIG_NETWORKING
jerry_value_t zjs_buffer_create_nbuf(struct net_pkt *net_buf, zjs_buffer_t **ret_buf);
#endif

#endif  // __zjs_buffer_h__
