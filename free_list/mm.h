#include <stdio.h>

extern void mm_init(void *heap, size_t heap_size);
extern void *mm_malloc(size_t size);
extern void mm_free(void *ptr);
