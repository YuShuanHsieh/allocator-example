#include <stdio.h> 
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>

#include "free_list.h"

void set_header(void *header, int size, bool allocated, bool prev_allocated)
{
    *(uint32_t *)header = size | allocated | (prev_allocated << 1);
}

void set_footer(void *footer, int size)
{
    *(uint32_t *)footer = size;
}

bool init_heap(void **heap)
{
    void *start;
    int default_size = DEFAULT_SIZE * ALIGNMENT;
    start = mmap(NULL, (HEADER_SIZE * 2) + default_size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
    if (start == MAP_FAILED)
    {
        perror("failed to mmap");
        return false;
    };
    *heap = start;
    set_header(start, default_size, false, true);
    set_header(NEXT_FRAG(start), 0, true, true);

    return true;
}

void *find_fragment(void *heap, size_t size)
{
    bool allocated;
    size_t fragment_size;
    void *fragment = heap;
    
    while(1)
    {
        allocated = FRAG_ALLOCATED(fragment);
        fragment_size = FRAG_SIZE(fragment);

        if (allocated == true && fragment_size == 0)
        {
            return NULL;
        } else if (allocated == true || fragment_size <= size + HEADER_SIZE)
        {
            fragment = NEXT_FRAG(fragment);
        } else {
            return fragment;
        }
    };
}

void *mm_malloc(void *heap, size_t size)
{
    int aligned_size = ALIGN(size);
    void *fragment = find_fragment(heap, aligned_size);

    if (fragment == NULL)
    {
        printf("space is not enough \n");
        return NULL;
    }

    size_t fragment_size = FRAG_SIZE(fragment);

    set_header(fragment, aligned_size, true, PREV_FRAG_ALLOCATED(fragment));
    set_header(NEXT_FRAG(fragment), fragment_size - aligned_size - HEADER_SIZE, false, true);

    return (char *)fragment + HEADER_SIZE;
}

void coalesce_fragment(void *fragment)
{
    int prev_size, next_size;
    bool prev = PREV_FRAG_ALLOCATED(fragment);
    bool next = FRAG_ALLOCATED(NEXT_FRAG(fragment));

    int size = FRAG_SIZE(fragment);

    if (prev == false && next == false)
    {
      void *prev_footer = (char *)fragment - HEADER_SIZE;
      prev_size = FRAG_SIZE(prev_footer);
      next_size = FRAG_SIZE(NEXT_FRAG(fragment));

      void *prev_header = (char *)fragment - prev_size - HEADER_SIZE;
      int new_size = (2 * HEADER_SIZE) + prev_size + next_size + size;
      set_header(prev_header, new_size, false, PREV_FRAG_ALLOCATED(prev_header));

      fragment = prev_header;

    } else if (prev == false)
    {
      prev_size = FRAG_SIZE((char *)fragment - HEADER_SIZE);
      void *prev_header = (char *)fragment - prev_size - HEADER_SIZE;

      int new_size = HEADER_SIZE + prev_size + size;
      set_header(prev_header, new_size, false, PREV_FRAG_ALLOCATED(prev_header));

      fragment = prev_header;

    } else if (next == false)
    {
      void *next_header = NEXT_FRAG(fragment);
      next_size = FRAG_SIZE(next_header);

      int new_size = HEADER_SIZE + next_size + size;
      set_header(fragment, new_size, false, PREV_FRAG_ALLOCATED(fragment));
      
    } else {
    }
}

void mm_free(void *content)
{
    if (content == NULL)
    {
        return;
    }
    void *fragment = (char *)content - HEADER_SIZE;
    set_header((uint32_t *)fragment, FRAG_SIZE(fragment), false, PREV_FRAG_ALLOCATED(fragment));
    void *footer = (char *)fragment + FRAG_SIZE(fragment);

    set_header((uint32_t *)footer, FRAG_SIZE(fragment), false, false);

    void *next = NEXT_FRAG(fragment);
    set_header((uint32_t *)next, FRAG_SIZE(next), FRAG_ALLOCATED(next), false);
    coalesce_fragment(fragment);
}

void print_fragment(void *heap)
{
    void *head = heap;
    size_t fragment_size;
    bool allocated;

    while(1)
    {  
        fragment_size = FRAG_SIZE(head);
        allocated = FRAG_ALLOCATED(head);

        if(allocated == true && fragment_size == 0)
        {
            break;
        }

        printf("fragment size: %u ", FRAG_SIZE(head));
        printf("allocated: %u ", FRAG_ALLOCATED(head));
        printf("\n");

        head = NEXT_FRAG(head);
    }
}

int main()
{
    void *heap;
    int result = init_heap(&heap);
    if (result == false)
    {
        return 0;
    }
    int *number = mm_malloc(heap, sizeof(int));
    *number = 4;

    int *number2 = mm_malloc(heap, sizeof(int));
    *number2 = 5;

    print_fragment(heap);

    mm_free(number);
    print_fragment(heap);
    mm_free(number2);
    print_fragment(heap);
}


