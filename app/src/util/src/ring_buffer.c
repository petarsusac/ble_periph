/**
 * A custom ring buffer implementation that overwrites older samples. Useful for
 * implementing moving averages and sliding windows. Currently supports only
 * floating point numbers as buffer items.
*/

#include "ring_buffer.h"

#include <string.h>

int ring_buffer_init(ring_buffer_t *p_ring_buf, float *p_buf, size_t size)
{
    if (p_ring_buf && p_buf)
    {
        p_ring_buf->head = 0;
        p_ring_buf->num_items = 0;
        p_ring_buf->size = size;
        p_ring_buf->sum = 0.0f;
        p_ring_buf->p_buf = p_buf;

        k_mutex_init(&p_ring_buf->mutex);

        return 0;
    }

    return -1;
}

int ring_buffer_put(ring_buffer_t *p_ring_buf, float item)
{
    size_t tail;

    if (p_ring_buf && p_ring_buf->p_buf)
    {
        k_mutex_lock(&p_ring_buf->mutex, K_FOREVER);

        if (p_ring_buf->num_items < p_ring_buf->size)
        {
            p_ring_buf->num_items += 1;
            p_ring_buf->sum += item;
        }
        else
        {
            p_ring_buf->sum = p_ring_buf->sum - p_ring_buf->p_buf[p_ring_buf->head] + item;
        }
        
        p_ring_buf->p_buf[p_ring_buf->head] = item;
        p_ring_buf->head = (p_ring_buf->head + 1) % p_ring_buf->size;

        k_mutex_unlock(&p_ring_buf->mutex);

        return 0;
    }

    return -1;
}

int ring_buffer_mov_avg(ring_buffer_t *p_ring_buf, float *p_res)
{
    if (p_ring_buf && p_res)
    {
        k_mutex_lock(&p_ring_buf->mutex, K_FOREVER);

        if (p_ring_buf->num_items != 0)
        {
            *p_res = p_ring_buf->sum / p_ring_buf->num_items;
        }
        else
        {
            *p_res = 0.0f;
        }

        k_mutex_unlock(&p_ring_buf->mutex);

        return 0;
    }

    return -1;
}

int ring_buffer_copy_inorder(ring_buffer_t *p_ring_buf, float *p_dest)
{
    if (p_ring_buf && p_ring_buf->p_buf && p_dest)
    {
        k_mutex_lock(&p_ring_buf->mutex, K_FOREVER);

        if (p_ring_buf->num_items < p_ring_buf->size)
        {
            // Buffer is not full yet - copy everything from 0 to head-1
            memcpy(p_dest, p_ring_buf->p_buf, p_ring_buf->head * sizeof(float));
        }
        else
        {
            // Head has circled around. Copy 2 times:
            // 1. From head to the end of the buffer
            // 2. From 0 to head-1
            memcpy(p_dest, &p_ring_buf->p_buf[p_ring_buf->head], (p_ring_buf->size - p_ring_buf->head) * sizeof(float));
            memcpy(&p_dest[p_ring_buf->size - p_ring_buf->head], p_ring_buf->p_buf, p_ring_buf->head * sizeof(float));
        }

        k_mutex_unlock(&p_ring_buf->mutex);

        return p_ring_buf->num_items;
    }

    return -1;
}

size_t ring_buffer_get_num_items(ring_buffer_t *p_ring_buf)
{
    size_t num_items = 0;

    if (p_ring_buf)
    {
        k_mutex_lock(&p_ring_buf->mutex, K_FOREVER);

        num_items = p_ring_buf->num_items;

        k_mutex_unlock(&p_ring_buf->mutex);
    }

    return num_items;
}
