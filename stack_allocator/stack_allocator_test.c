#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stack_allocator.h"

int main()
{
  struct game_resource *resource_a, *resource_b;
  struct stackAllocator allocator;
  
  EXPECT(init_allocator(&allocator, 4096), true, "init allocator failed");
  
  resource_a = allocate(&allocator, sizeof(struct game_resource));
  resource_a->level = 5;
  strcpy(resource_a->name, "level1");
  printf("%s allocated\n", resource_a->name);

  resource_b = allocate(&allocator, sizeof(struct game_resource));
  resource_b->level = 3;
  strcpy(resource_b->name, "level2");
  printf("%s allocated\n", resource_b->name);

  EXPECT(deallocate(&allocator, resource_b), true, "deallocate failed");
  EXPECT(deallocate(&allocator, resource_a), true, "deallocate failed");

  return 0; 
}