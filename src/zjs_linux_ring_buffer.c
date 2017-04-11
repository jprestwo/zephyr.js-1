// Copyright (c) 2015-2017-2016, Intel Corporation.

/*
 * This ring buffer implementation was taken from the Zephyr source and
 * slightly modified to run on Linux.
 */

#include "zjs_linux_port.h"

#ifndef likely
#define likely(x)   __builtin_expect((long)!!(x), 1L)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect((long)!!(x), 0L)
#endif

struct ring_element {
    uint32_t  type   :16; /*< Application-specific */
    uint32_t  length :8;  /*< length in 32-bit chunks */
    uint32_t  value  :8;  /*< Room for small integral values */
};

static int get_space(struct zjs_port_ring_buf *buf)
{
    if (buf->head == buf->tail) {
        return buf->size - 1;
    }

    if (buf->tail < buf->head) {
        return buf->head - buf->tail - 1;
    }

    /* buf->tail > buf->head */
    return (buf->size - buf->tail) + buf->head - 1;
}

void zjs_port_ring_buf_init(struct zjs_port_ring_buf *buf,
                            uint32_t size,
                            uint32_t *data)
{
    int i;
    for (i = 0; i < 20; ++i) {
        if (size == (1 << i) * 4) {
            break;
        } else if (size < (1 << i) * 4) {
            ERR_PRINT("size %u is not power of 2, setting size to %u\n", size, (1 << i) * 4);
            break;
        }
    }

    DBG_PRINT("ring buffer size: %u\n", (1 << i) * 4);

    buf->head = 0;
    buf->tail = 0;
    buf->size = (1 << (i));
    buf->mask = (1 << (i)) - 1;
    buf->buf = data;
}

int zjs_port_ring_buf_get(struct zjs_port_ring_buf *buf,
                          uint16_t *type,
                          uint8_t *value,
                          uint32_t *data,
                          uint8_t *size32)
{
    struct ring_element *header;
    uint32_t i, index;

    if (buf->head == buf->tail) {
        return -EAGAIN;
    }

    header = (struct ring_element *) &buf->buf[buf->head];

    if (header->length > *size32) {
        *size32 = header->length;
        return -EMSGSIZE;
    }

    *size32 = header->length;
    *type = header->type;
    *value = header->value;

    if (likely(buf->mask)) {
        for (i = 0; i < header->length; ++i) {
            index = (i + buf->head + 1) & buf->mask;
            data[i] = buf->buf[index];
        }
        buf->head = (buf->head + header->length + 1) & buf->mask;
    } else {
        for (i = 0; i < header->length; ++i) {
            index = (i + buf->head + 1) % buf->size;
            data[i] = buf->buf[index];
        }
        buf->head = (buf->head + header->length + 1) % buf->size;
    }



    return 0;
}

int zjs_port_ring_buf_put(struct zjs_port_ring_buf *buf,
                          uint16_t type,
                          uint8_t value,
                          uint32_t *data,
                          uint8_t size32)
{
    uint32_t i, space, index, rc = -1;

    space = get_space(buf);
    if (space >= (size32 + 1)) {
        struct ring_element *header =
                (struct ring_element *)&buf->buf[buf->tail];
        header->type = type;
        header->length = size32;
        header->value = value;

        if (likely(buf->mask)) {
            for (i = 0; i < size32; ++i) {
                index = (i + buf->tail + 1) & buf->mask;
                buf->buf[index] = data[i];
            }
            buf->tail = (buf->tail + size32 + 1) & buf->mask;
        } else {
            for (i = 0; i < size32; ++i) {
                index = (i + buf->tail + 1) % buf->size;
                buf->buf[index] = data[i];
            }
            buf->tail = (buf->tail + size32 + 1) % buf->size;
        }
        rc = 0;
    } else {
        rc = -EMSGSIZE;
    }

    return rc;
}
