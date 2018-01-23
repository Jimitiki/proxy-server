#include "cache.h"

cache_object * init_object(unsigned char * bytes, char * name, int size);
void push(cache *c, cache_object *object);
void pop(cache *c);
void print_object(cache_object *object);

cache *init_cache()
{
	cache *new_cache = (cache *) malloc (sizeof(cache));
	new_cache->head = NULL;
	new_cache->tail = NULL;
	new_cache->size = 0;
	return new_cache;
}

void destroy_cache(cache *c)
{
	while (c->head != NULL)
	{
		pop(c);
	}
	free(c);
}

cache_object *read_cache(cache *c, char * name)
{
	printf("READING OBJECT %s\n", name);
	cache_object *it = c->head;
	while(it != NULL && strcmp(it->name, name))
	{
		it = it->next;
	}
	if (it == NULL)
	{
		return NULL;
	}
	
	if (it->prev != NULL)
	{
		printf("Object is not the head\n");
		if (it->next != NULL)
		{
			printf("Object is not the tail\n");
			it->next->prev = it->prev;
		}
		it->prev->next = it->next;
		it->next = c->head;
		c->head = it;
	}
	
	return it;
}

int cache_bytes(cache *c, unsigned char * bytes, char * name, int size)
{
	if (size > MAX_OBJECT_SIZE) {
		return 0;
	}
	while (c->size + size >= MAX_CACHE_SIZE)
	{
		pop(c);
	}
	cache_object *object = init_object(bytes, name, size);
	push(c, object);
	return 1;
}

void print_cache(cache *c)
{
	printf("+++++++++++CACHE+++++++++++\n");
	printf("SIZE: %d\tREMAINING SPACE: %d\n", c->size, MAX_CACHE_SIZE - c->size);
	cache_object *it = c->head;
	if (it == NULL)
	{
		printf("CACHE IS CURRENTLY EMPTY\n\n");
		return;
	}
	printf("=======CACHE OBJECTS=======\n");
	while(it != NULL)
	{
		print_object(it);
		printf("\n");
		it = it->next;
	}
	printf("\n");
}

cache_object * init_object(unsigned char * bytes, char * name, int size)
{
	cache_object *object = (cache_object *) malloc(sizeof(cache_object));
	object->name = name;
	object->bytes = bytes;
	object->size = size;
	
	object->prev = NULL;
	object->next = NULL;
	return object;
}

void push(cache *c, cache_object *object)
{
	object->next = c->head;
	if (object->next != NULL)
	{
		object->next->prev = object;
	}
	c->head = object;
	if (c->tail == NULL)
	{
		c->tail = object;
	}
	c->size += object->size;
}

void pop(cache *c)
{
	if (c->tail == NULL)
	{
		return;
	}
	cache_object * cur_tail = c->tail;
	c->tail = cur_tail->prev;
	if (c->tail == NULL)
	{
		c->head = NULL;
	}
	else
	{
		c->tail->next = NULL;
	}
	c->size -= cur_tail->size;
	free(cur_tail);
}

void print_object(cache_object *object)
{
	if (object == NULL)
	{
		printf("NULL\n");
		return;
	}
	printf("Name: %s\n", object->name);
	printf("Size: %d bytes\n", object->size);
	printf("Bytes: %s\n", object->bytes);
}
