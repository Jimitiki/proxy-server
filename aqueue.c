#include "aqueue.h"

a_queue *init_queue(unsigned int capacity)
{
	a_queue *q = (a_queue *) malloc(sizeof(a_queue));
	q->capacity = capacity;
	q->head = 0;
	q->tail = 0;
	
	q->queue = calloc(capacity, sizeof(void *));
}

void destroy_queue(a_queue *queue)
{
	free(queue->queue);
	free(queue);
}

int push_queue(a_queue *queue, void *item)
{
	if (queue->tail % queue->capacity == queue->head % queue->capacity - 1)
	{
		return 0;
	}
	int index = queue->tail % queue->capacity;
	queue->queue[index] = item;
	queue->tail++;
	return 1;
}

void *pop_queue(a_queue *queue)
{
	if (queue->tail == queue->head)
	{
		return NULL;
	}
	int index = queue->head % queue->capacity;
	queue->head++;
	return queue->queue[index];
}
