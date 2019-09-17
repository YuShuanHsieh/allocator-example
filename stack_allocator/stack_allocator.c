#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <stdbool.h>
#include "stack_allocator.h"

bool init_allocator(struct stackAllocator *allocator, size_t length)
{
  char *addr;
  allocator->length = length;
  allocator->head = NULL;
  pthread_mutex_init(&allocator->mu, NULL);

  addr = (char *)mmap(NULL, allocator->length, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  
  if (addr == MAP_FAILED) {
    perror("init allocator");
    return false;
  }

  allocator->top_addr = addr;
  allocator->bottom_addr = allocator->top_addr + allocator->length;
  
  return true;
}

void *allocate(struct stackAllocator *allocator, size_t n)
{
  pthread_mutex_lock(&allocator->mu);
  char *temp = allocator->top_addr;
  
  if (temp + n >= allocator->bottom_addr) {
    DEBUG("No more space for allocation");
    temp = NULL;
    goto unlock;
  }
  allocator->top_addr += n;

  size_t size = sizeof(struct allocate_state);
  allocator->bottom_addr -= size;

  struct allocate_state* state = (struct allocate_state *) allocator->bottom_addr;
  state->next = NULL;
  state->addr = temp;
  state->n = n;

  if (allocator->head == NULL)
  {
    allocator->head = state;
  }
  else {
    state->next = allocator->head;
    allocator->head = state;
  }

unlock:
  pthread_mutex_unlock(&allocator->mu);

  return temp;
}

bool deallocate(struct stackAllocator *allocator, void *addr)
{
  bool result = false;
  pthread_mutex_lock(&allocator->mu);

  if (allocator->head == NULL) {
    DEBUG("allocate state is null");
    goto unlock;
  }
  if (allocator->head->addr != addr) {
    DEBUG("addrsss mismatched: target %p state %p", addr, allocator->head->addr);
    goto unlock;
  }

  allocator->top_addr -= allocator->head->n;
  size_t size = sizeof(struct allocate_state);
  allocator->bottom_addr += size;
  
  allocator->head = allocator->head->next;
  result = true;

unlock:
  pthread_mutex_unlock(&allocator->mu);

  return result;
}
