#ifndef CSAPP_H_
#define CSAPP_H_

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

struct cache_object{
	struct cache_object * prev;
	struct cache_object * next;
	char * name;
	unsigned char * bytes;
	int size;
};

typedef struct cache_object cache_object;

typedef struct {
	cache_object * head;
	cache_object * tail;
	int size;
} cache;

cache *init_cache();
void destroy_cache(cache *c);
int cache_bytes(cache *c, unsigned char * bytes, char * name, int size);
cache_object *read_cache(cache *c, char * name);
void print_cache(cache *c);
void test_cache();

#endif
