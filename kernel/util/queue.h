#ifndef UTIL_QUEUE_H
#define UTIL_QUEUE_H

#include <stdint.h>
#include "kernel/util/list.h"

struct queue
{
	struct list_head *qhead;
	uint32_t number_of_items;
};

void queue_push(struct queue *q, void *data);
void *queue_pop(struct queue *q);
void *queue_peek(struct queue *q);

#endif
