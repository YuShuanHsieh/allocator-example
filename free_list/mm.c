#include <stdint.h>
#include <stdio.h> 
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>

#include "mm.h"

typedef uint64_t header;

#define HEADER_SIZE sizeof(header)
#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))
#define BLOCK_SIZE(header_ptr) (*(uint64_t *) header_ptr & ~0xF)
#define BLOCK_ALLOCATED(header_ptr) (*(uint64_t *) header_ptr & 1)
#define BLOCK_FOOTER(header_ptr) ((char *)(header_ptr) + BLOCK_SIZE(header_ptr) - HEADER_SIZE)
#define PREV_BLOCK_ALLOCATED(header_ptr) ((*(uint64_t *) header_ptr & 2) >> 1)
#define NEXT_BLOCK(header_ptr)((char *)(header_ptr) + BLOCK_SIZE(header_ptr))

void *area;

void set_header(void *header, int size, bool allocated, bool prev_allocated)
{
    *(uint64_t *)header = size | allocated | (prev_allocated << 1);
}

void set_footer(void *footer, int size)
{
    *(uint64_t *)footer = size;
}

void mm_init(void *heap, size_t heap_size)
{ 
  area = heap + HEADER_SIZE;
  heap_size = heap_size - 2 * HEADER_SIZE;
  set_header(area, heap_size, false, true);
  set_header(NEXT_BLOCK(area), 0, true, true);

}

void *find_free_block(void *heap, size_t size)
{
    bool allocated;
    size_t block_size;
    void *block = heap;

    while(1)
    {
        allocated = BLOCK_ALLOCATED(block);
        block_size = BLOCK_SIZE(block);

        if (block_size == 0)
        {
            return NULL;
        } else if (allocated == true || block_size < size)
        {
            block = NEXT_BLOCK(block);
        } else {
            return block;
        }
    };
}

void print_fragment(void *heap)
{
    void *head = heap;
    size_t fragment_size;
    bool allocated;

    while(1)
    {  
        fragment_size = BLOCK_SIZE(head);
        allocated = BLOCK_ALLOCATED(head);

        if(fragment_size == 0)
        {
            break;
        }

        printf(" [%zu, %d] ", fragment_size, allocated);
        head = NEXT_BLOCK(head);
    }
    printf("\n");
}

void *mm_malloc(size_t size)
{
  if (size == 0)
  {
    return NULL;
  }

  int aligned_size = ALIGN(size + HEADER_SIZE);
  void *block = find_free_block(area, aligned_size);

  if (block == NULL)
  {
      printf("space is not enough \n");
      return NULL;
  }

  size_t block_size = BLOCK_SIZE(block);
  
  size_t remainder_size = block_size - aligned_size;

  if (remainder_size > ALIGN(HEADER_SIZE))
  {
    set_header(block, aligned_size, true, PREV_BLOCK_ALLOCATED(block));
    set_header(NEXT_BLOCK(block), remainder_size, false, true);
  } else {
    void *next_header = NEXT_BLOCK(block);
    set_header(block, BLOCK_SIZE(block), true, PREV_BLOCK_ALLOCATED(block));
    set_header(next_header, BLOCK_SIZE(next_header), BLOCK_ALLOCATED(next_header), true);
  }

  return (char *)(block) + HEADER_SIZE;
}


void coalesce_blocks(void *block)
{
    size_t size, prev_size, next_size;
    bool prev = PREV_BLOCK_ALLOCATED(block);
    bool next = BLOCK_ALLOCATED(NEXT_BLOCK(block));

    size = BLOCK_SIZE(block);

    if (prev == false && next == false)
    {
      void *prev_footer = (char *)block - HEADER_SIZE;
      prev_size = BLOCK_SIZE(prev_footer);
      next_size = BLOCK_SIZE(NEXT_BLOCK(block));

      void *prev_header = (char *)block - prev_size;
      int new_size = prev_size + next_size + size;
      set_header(prev_header, new_size, false, PREV_BLOCK_ALLOCATED(prev_header));
      set_footer(BLOCK_FOOTER(prev_header), new_size);

      block = prev_header;

    } else if (prev == false)
    {
      void *prev_footer = (char *)block - HEADER_SIZE;
      prev_size = BLOCK_SIZE(prev_footer);
      void *prev_header = (char *)block - prev_size;

      int new_size = prev_size + size;
      set_header(prev_header, new_size, false, PREV_BLOCK_ALLOCATED(prev_header));
      set_footer(BLOCK_FOOTER(prev_header), new_size);

      block = prev_header;

    } else if (next == false)
    {
      next_size = BLOCK_SIZE(NEXT_BLOCK(block));

      int new_size = next_size + size;
      set_header(block, new_size, false, PREV_BLOCK_ALLOCATED(block));
      set_footer(BLOCK_FOOTER(block), new_size);
      
    } else {
    }
}

void mm_free(void *payload)
{
  if (payload == NULL)
  {
      return;
  }
  void *header = (char *)payload - HEADER_SIZE;
  set_header(header, BLOCK_SIZE(header), false, PREV_BLOCK_ALLOCATED(header));
  
  void *footer = BLOCK_FOOTER(header);
  set_footer(footer, BLOCK_SIZE(header));

  void *next = NEXT_BLOCK(header);
  set_header(next, BLOCK_SIZE(next), BLOCK_ALLOCATED(next), false);

  coalesce_blocks(header);
}
