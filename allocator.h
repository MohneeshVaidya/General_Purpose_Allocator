#ifndef allocator_h
#define allocator_h


#include <stddef.h>


typedef struct Allocator Allocator;


Allocator* allocator_create();

void allocator_destroy(Allocator* allocator);

void allocator_reset(Allocator* allocator);

void* allocator_malloc(Allocator* allocator, size_t size);

void* allocator_calloc(Allocator* allocator, size_t count, size_t object_size);

void* allocator_realloc(Allocator* allocator, void* pointer, size_t new_size);

void allocator_free(Allocator* allocator, void* pointer);


#endif
