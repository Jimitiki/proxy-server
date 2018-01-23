#ifndef AQUEUE_H_
#define AQUEUE_H_

#include "csapp.h"

typedef struct {
	void **queue;
	unsigned int head;
	unsigned int tail;
	unsigned int capacity;
} a_queue;

a_queue *init_queue(unsigned int capacity);

void destroy_queue(a_queue *queue);

int push_queue(a_queue *queue, void *item);

void *pop_queue(a_queue *queue);
 
#endif

