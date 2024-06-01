#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

#include <stddef.h>

typedef struct ring_buffer
{
    size_t head;
    size_t size;
    size_t num_items;
    float sum;
    float *p_buf;
} ring_buffer_t;

int ring_buffer_init(ring_buffer_t *p_ring_buf, float *p_buf, size_t size);

int ring_buffer_put(ring_buffer_t *p_ring_buf, float item);

int ring_buffer_mov_avg(ring_buffer_t *p_ring_buf, float *p_res);

int ring_buffer_copy_inorder(ring_buffer_t *p_ring_buf, float *p_dest);

#endif /* _RING_BUFFER_H_ */