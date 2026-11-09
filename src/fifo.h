/*
 * src/fifo.h
 *
 * generic queue: a linked list with a head and a tail
 */

#ifndef FIFO_H
#define FIFO_H

#include <stddef.h>

struct list
{
    void *data; // value stored in this node
    struct list *next; // next node, NULL for the last one
};

struct fifo
{
    struct list *head; // first node, NULL if the queue is empty
    struct list *tail; // last node, used to push at the end in O(1)
    size_t size; // number of elements in the queue
};

/* create an empty queue, never returns NULL */
struct fifo *fifo_init(void);

/* add an element at the end of the queue */
void fifo_push(struct fifo *fifo, void *elt);

/* free the queue and call free_elt on each element */
void fifo_destroy(struct fifo *fifo, void (*free_elt)(void *));

#endif /* ! FIFO_H */
