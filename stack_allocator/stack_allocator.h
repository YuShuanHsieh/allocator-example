#ifndef STACK_ALLOCATOR_H

#include <stdbool.h>
#include <pthread.h>

#ifdef DEBUG
#define DEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#else
#define DEBUG(fmt, args...)
#endif

#define EXPECT(result, expected, msg) do { \
  if(result != expected) { \
    fprintf(stderr, "DEBUG: %s\n",msg); \
    exit(EXIT_FAILURE); \
   } \
} while(0) \

struct allocate_state {
  struct allocate_state *next;
  char *addr;
  size_t n;
};

struct stackAllocator {
  char *top_addr;
  char *bottom_addr;
  size_t length;
  struct allocate_state *head;
  pthread_mutex_t mu;
};

struct game_resource {
  int level;
  char name[50];
};

extern bool init_allocator(struct stackAllocator *allocator, size_t length);
extern void *allocate(struct stackAllocator *allocator, size_t n);
extern bool deallocate(struct stackAllocator *allocator, void *addr);

#endif