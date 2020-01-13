#include <stdint.h>

typedef uint32_t header;

#define HEADER_SIZE sizeof(header)
#define DEFAULT_SIZE 10
#define ALIGNMENT 4
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define FRAG_SIZE(fragment) (*(uint32_t *) fragment & ~3)
#define FRAG_ALLOCATED(fragment) (*(uint32_t *) fragment & 1)
#define FRAG_FOOTER(fragment) ((char *)fragment - FRAG_SIZE(fragment))
#define PREV_FRAG_ALLOCATED(fragment) ((*(uint32_t *) fragment & 2) >> 1)
#define NEXT_FRAG(fragment)((char *)fragment + FRAG_SIZE(fragment) + HEADER_SIZE)

void set_header(void *header, int size, bool allocated, bool prev_allocated);
void set_footer(void *footer, int size);
bool init_heap(void **heap);
void *find_fragment(void *heap, size_t size);
void *mm_malloc(void *heap, size_t size);
void print_fragment(void *heap);