/*
 * src/fifo.c
 *
 * implementation of the queue
 *
 * see src/fifo.h
 */

#include "fifo.h"

#include <stdlib.h>

#include "safe_memory.h"

struct fifo *fifo_init(void)
{
    struct fifo *queue = safe_malloc(sizeof(struct fifo));
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;

    return queue;
}

void fifo_push(struct fifo *fifo, void *elt)
{
    struct list *new_node = safe_malloc(sizeof(struct list));
    new_node->data = elt;
    new_node->next = NULL;

    // the first node becomes the head, the others go after the tail
    if (fifo->size == 0)
        fifo->head = new_node;
    else
        fifo->tail->next = new_node;

    fifo->tail = new_node;
    fifo->size++;
}

void fifo_destroy(struct fifo *fifo, void (*free_elt)(void *))
{
    if (!fifo)
        return;

    struct list *curr = fifo->head;

    while (curr)
    {
        // keep the node because curr must move before the free
        struct list *to_free = curr;
        curr = curr->next;
        free_elt(to_free->data);
        free(to_free);
    }

    free(fifo);
}
