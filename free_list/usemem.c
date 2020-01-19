#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include "mm.h"

static void alloc_single(int n, int s, int iters, int compact);
static void alloc_singles(int n, int s, int iters, int compact);
static void alloc_excessive(int n, int s, int iters, int compact);
static void alloc_shrinking(int n, int s, int iters, int compact);
static void alloc_growing(int n, int s, int iters, int compact);
static void alloc_timing(int n, int s, int iters, int compact);

int main(int argc, char **argv)
{
  const char *which = NULL;
  int n = 1000;
  int s = 16;
  int iters = 10;
  int compact = 0;
  int i;

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--n")) {
      if (i+1 >= argc) {
        fprintf(stderr, "%s: missing number argument for --n\n", argv[0]);
        exit(1);
      }
      n = atoi(argv[i+1]);
      if (n < 0) {
        fprintf(stderr, "%s: number after --n cannot be negative\n", argv[0]);
        exit(1);
      }
      i++;
    } else if (!strcmp(argv[i], "--s")) {
      if (i+1 >= argc) {
        fprintf(stderr, "%s: missing number argument for --s\n", argv[0]);
        exit(1);
      }
      s = atoi(argv[i+1]);
      if (s < 0) {
        fprintf(stderr, "%s: number after --s cannot be negative\n", argv[0]);
        exit(1);
      }
      i++;
    } else if (!strcmp(argv[i], "--iters")) {
      if (i+1 >= argc) {
        fprintf(stderr, "%s: missing number argument for --iters\n", argv[0]);
        exit(1);
      }
      iters = atoi(argv[i+1]);
      if (iters < 0) {
        fprintf(stderr, "%s: number after --iters cannot be negative\n", argv[0]);
        exit(1);
      }
      i++;
    } else if (!strcmp(argv[i], "--compact")) {
      compact = 1;
    } else if (!strcmp(argv[i], "--single")) {
      which = "single";
    } else if (!strcmp(argv[i], "--singles")) {
      which = "singles";
    } else if (!strcmp(argv[i], "--excessive")) {
      which = "excessive";
    } else if (!strcmp(argv[i], "--shrinking")) {
      which = "shrinking";
    } else if (!strcmp(argv[i], "--growing")) {
      which = "growing";
    } else if (!strcmp(argv[i], "--timing")) {
      which = "timing";
    } else {
      fprintf(stderr, "%s: unrecognized argument: %s\n", argv[0], argv[i]);
      exit(1);
    }
  }

  if (!which) {
    fprintf(stderr, ("%s: select a test: --single, --singles, --excessive,"
                     " --shrinking, --growing, or --timing\n"),
            argv[0]);
    exit(1);
  }

  printf("Running %s with n=%d, s=%d, iters=%d\n", which, n, s, iters);
  fflush(stdout);

  if (!strcmp(which, "single"))
    alloc_single(n, s, iters, compact);
  else if (!strcmp(which, "singles"))
    alloc_singles(n, s, iters, compact);
  else if (!strcmp(which, "excessive"))
    alloc_excessive(n, s, iters, compact);
  else if (!strcmp(which, "shrinking"))
    alloc_shrinking(n, s, iters, compact);
  else if (!strcmp(which, "growing"))
    alloc_growing(n, s, iters, compact);
  else if (!strcmp(which, "timing"))
    alloc_timing(n, s, iters, compact);

  printf("Passed\n");
  
  return 0;
}

/*************************************************************/
/* helpers                                                   */
/*************************************************************/

#define ALIGNMENT 16
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

#define IS_ODD(n) ((n) & 1)
#define IS_EVEN(n) (!((n) & 1))

static void  *the_heap;
static size_t the_heap_size;

static void fill(void *p, int key, int s)
{
  memset(p, key & 255, s);
}

static void check(void *p, int key, int s)
{
  int i;
 
  for (i = 0; i < s; i++) {
    if (((unsigned char *)p)[i] != (key & 255)) {
      fprintf(stderr,
              "memory changed incorrectly; expected %d, found %d\n",
              (key & 255),
              ((unsigned char *)p)[i]);
      exit(1);
    }
  }
}

static void *checked_malloc(size_t s, int fail_ok)
{
  void *p;

  p = mm_malloc(s);
  
  if (!p) {
    if (fail_ok) {
      return NULL;
    } else {
      fprintf(stderr, "malloc incorrectly ran out of memory\n");
      exit(1);
    }
  }

  if ((long)p & 0xF) {
    fprintf(stderr, "malloc result is not 16-byte aligned\n");
    exit(1);
  }

  if ((p < the_heap) || (p >= (the_heap + the_heap_size))) {
    fprintf(stderr,
            "malloc result %p is not within the heap area [%p, %p) given to mm_init\n",
            p, the_heap, the_heap + the_heap_size);
    exit(1);
  }
  if ((p+s) > (the_heap + the_heap_size)) {
    fprintf(stderr,
            "malloc result %p ends at %p, beyond heap area [%p, %p) given to mm_init\n",
            p, p+s, the_heap, the_heap + the_heap_size);
    exit(1);
  }
  
  return p;
}

static void init_heap(int n, int min_size, int total_size, int compact)
{
  long max_pad = ALIGN(min_size) - min_size;
  long overhead = (compact ? 16 : 32);
  long heap_size = (n * (overhead + max_pad)) + total_size + 64;
  long ps = getpagesize();
  void *heap;

  /* round up to page size: */
  heap_size = (heap_size + (ps - 1)) & ~(ps-1);

  /* add pre and post page */
  heap = mmap(0, heap_size + ps*2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

  /* make pre and post page unreadable and unwritable */
  mprotect(heap, ps, 0);
  heap += ps;
  mprotect(heap+heap_size, ps, 0);

  mm_init(heap, heap_size);

  the_heap = heap;
  the_heap_size = heap_size;
}

static long now()
{
  struct rusage use;
  long s, u;

  getrusage(RUSAGE_SELF, &use);

  s = use.ru_utime.tv_sec + use.ru_stime.tv_sec;
  u = use.ru_utime.tv_usec + use.ru_stime.tv_usec;

  return s * 1000 + u / 1000;
}

/*************************************************************/
/* single: allocate an object of size s, and free it.        */
/*  This test passes for a trivial allocator that directly   */
/*  calls mem_map and mem_unmap.                             */
/*************************************************************/

void alloc_single(int n, int s, int iters, int compact)
{
  int j;
  void *p, *q;

  init_heap(1, s, s, compact);

  for (j = 0; j < iters; j++) {
    p = checked_malloc(s, 0);
    fill(p, j, s);

    /* Try to get a second block, even though that's more than we said
       we'd try. The allocator should succeed or fail gacefully. */
    q = checked_malloc(s, 1);
    if (q == NULL) {
      /* Ok - out of memory */
    } else {
      fill(q, j+1, s);
      check(q, j+1, s);
      mm_free(q);
    }

    check(p, j, s);
    mm_free(p);
  }
}

/*************************************************************/
/* singles: allocate n objects of size s, freeing them all.  */
/*  This test passes with a non-coalescing allocator.        */
/*************************************************************/

void alloc_singles(int n, int s, int iters, int compact)
{
  int i, j;
  void *p[n];

  init_heap(n, s, n*s, compact);

  for (j = 0; j < iters; j++) {
    /* Allocate n s-byte objects */
    for (i = 0; i < n; i++) {
      p[i] = checked_malloc(s, 0);
      fill(p[i], i+j, s);
    }

    /* Check all n objects */
    for (i = 0; i < n; i++) {
      check(p[i], i+j, s);
    }

    /* Try out different deallocation orders */
    switch (j & 3) {
    case 0:
      /* Free in order of allocation */
      for (i = 0; i < n; i++)
        mm_free(p[i]);
      break;
    case 1:
      /* Free in reverse order of allocation */
      for (i = n; i--; )
        mm_free(p[i]);
      break;
    case 2:
      /* Free odd objects in order of allocation */
      for (i = 0; i < n; i++) {
        if (IS_ODD(i))
          mm_free(p[i]);
      }
      
      /* Check even objects */
      for (i = 0; i < n; i++) {
        if (IS_EVEN(i))
          check(p[i], i+j, s);
      }
      
      /* Free even objects in reverse order */
      for (i = n; i--; ) {
        if (IS_EVEN(i))
          mm_free(p[i]);
      }
      break;
    case 3:
      {
        int n_ish;
        int k;

        /* Find a number like n, but a power of 2, so that 7 and this
           number are relatively prime */
        n_ish = 1;
        while (n_ish * 2 < n)
          n_ish *= 2;

        /* Remove one fourth of them */
        k = 17 % n_ish;
        for (i = 0; i < n/2; i++) {
          mm_free(p[k]);
          p[k] = NULL;
          k = (k + 7) % n_ish;
        }
        
        /* Check remaining objects */
        for (i = 0; i < n; i++) {
          if (p[i])
            check(p[i], i+j, s);
        }

        if (j & 0x4) {
          /* Allocate replacements for freed objects */
          k = 17 % n_ish;
          for (i = 0; i < n/2; i++) {
            p[k] = checked_malloc(s, 0);
            fill(p[k], k+j, s);
            k = (k + 7) % n_ish;
          }

          /* Check and free all objects */
          for (i = 0; i < n; i++) {
            check(p[i], i+j, s);
            mm_free(p[i]);
          }
        } else {
          /* Free remaining objects */
          for (i = 0; i < n; i++) {
            if (p[i])
              mm_free(p[i]);
          }
        }
      }
      break;
      
    }

    /* We've freed everything, so we should be able to allocate
       them all again */
  }
}

/*************************************************************/
/* excessive: allocate objects unless the allocator returns  */
/*            NULL, then free allocated objects              */
/*  This test should pass for pretty much any allocator.     */
/*************************************************************/

void alloc_excessive(int n, int s, int iters, int compact)
{
  int i, j, sz;
  void **p;
  size_t p_size = n + 1;
  
  init_heap(n, s, n*s, compact);
  p = malloc(p_size * sizeof(void*));
  
  for (j = 0; j < iters; j++) {
    /* Allocate objects until failure */
    for (i = 0; 1; i++) {
      if (i >= p_size) {
        void *new_p;
        size_t new_p_size = 2 * p_size;
        new_p = malloc(new_p_size * sizeof(void*));
        memcpy(new_p, p, p_size * sizeof(void*));
        free(p);
        p_size = new_p_size;
        p = new_p;
      }
      
      sz = s + (i + j) % s;
      p[i] = checked_malloc(sz, 1);
      if (p[i])
        fill(p[i], i+j, sz);
      else
        break;
    }
    n = i;

    /* Free all */
    for (i = 0; i < n; i++) {
      sz = s + (i + j) % s;
      check(p[i], i+j, sz);
      mm_free(p[i]);
    }
  }
}

/*************************************************************/
/* shrinking: allocate 1 object of size n*s, free, then      */
/*            allocate 2 objects of size (n/2)*s, free, ...  */
/*            The iteration argument is not used.            */
/*   This test needs splitting of blocks to pass.            */
/*************************************************************/

void alloc_shrinking(int n, int s, int iters, int compact)
{
  int i, k, n2, s2;
  void *p[n];
  
  init_heap(n, 1, n*s, compact);

  for (k = 1; (1<<k) <= n; k++) {
    n2 = (1<<(k-1));
    s2 = (n>>k)*s;
    
    /* Allocate n2 s2-byte objects */
    for (i = 0; i < n2; i++) {
      p[i] = checked_malloc(s2, 0);
      fill(p[i], i, s2);
    }

    /* Check all objects */
    for (i = 0; i < n2; i++) {
      check(p[i], i, s2);
    }

    /* Free odd objects in order of allocation */
    for (i = n2; i--; ) {
      if (IS_ODD(i))
        mm_free(p[i]);
    }

    /* Check even objects */
    for (i = 0; i < n2; i++) {
      if (IS_EVEN(i))
        check(p[i], i, s2);
    }
  
    /* Free even objects in reverse order */
    for (i = n2; i--; ) {
      if (IS_EVEN(i))
        mm_free(p[i]);
    }

    /* We've freed everything, so we should be able to allocate
       them all again */
  }
}

/*************************************************************/
/* growing: allocate n objects of size s, free them, then    */
/*          allocate n/2 objects of size s*2, free them, ... */
/*   This test needs coalescing to pass for a large enough   */
/*   value of n.                                             */
/*************************************************************/

void alloc_growing(int n, int s, int iters, int compact)
{
  int i, j, k;
  void *p[n];
  
  init_heap(n, s, n*s, compact);

  for (j = 0; j < iters; j++) {
    for (k = 0; n >> k > 0; k++) {
      /* Allocate n>>k (s<<k)-byte objects */
      for (i = 0; i < n>>k; i++) {
        p[i] = checked_malloc(s<<k, 0);
        fill(p[i], i+j, s<<k);
      }

      /* Check all n objects */
      for (i = 0; i < n>>k; i++) {
        check(p[i], i+j, s<<k);
      }

      /* Free odd objects in order of allocation */
      for (i = n>>k; i--; ) {
        if (IS_ODD(i))
          mm_free(p[i]);
      }

      /* Check even objects */
      for (i = 0; i < n>>k; i++) {
        if (IS_EVEN(i))
          check(p[i], i+j, s<<k);
      }
  
      /* Free even objects in reverse order */
      for (i = n>>k; i--; ) {
        if (IS_EVEN(i))
          mm_free(p[i]);
      }

      /* We've freed everything, so we should be able to allocate
         them all again */
    }
  }
}

/*************************************************************/
/* timing: allocate n objects of size s, free half of them,  */
/*        reallocate, free, etc. --- checking whether the    */
/*        allocation time appears to be constant time.       */
/*  This test should pass easily with an explicit free list. */
/*  Also, init_heap() must work a second time.               */
/*************************************************************/

void do_alloc_timing(int n, int s, int iters, int compact)
{
  int i, j, sz;
  void *p[n];
  
  init_heap(n, 2*s, n*2*s, compact);
  
  for (j = 0; j < iters; j++) {
    /* Allocate n s-byte objects */
    for (i = 0; i < n; i++) {
      sz = s + (i + j) % s;
      p[i] = checked_malloc(sz, 0);
      fill(p[i], i+j, sz);
    }

    /* Free odd objects */
    for (i = 0; i < n; i++) {
      if (IS_ODD(i))
        mm_free(p[i]);
    }

    /* Reallocate odd objects */
    for (i = 0; i < n; i++) {
      if (IS_ODD(i)) {
        sz = s + (i + j) % s;
        p[i] = checked_malloc(sz, 0);
        fill(p[i], i+j, sz);
      }
    }
      
    /* Free all */
    for (i = 0; i < n; i++) {
      sz = s + (i + j) % s;
      check(p[i], i+j, sz);
      mm_free(p[i]);
    }

    /* We've freed everything, so we should be able to allocate
       them all again */
  }
}

void alloc_timing(int n, int s, int iters, int compact)
{
  long time1, time2;

  time1 = now();
  do_alloc_timing(n, s, iters, compact);
  time1 = now() - time1;
  time1++; /* make sure it's not 0 */
  
  time2 = now();
  do_alloc_timing(4*n, s, iters, compact);
  time2 = now() - time2;
  
  if (time2 > 10 * time1) {
    fprintf(stderr, "Using 4*n takes more than 10 times as long: %ld vs. %ld\n",
            time1, time2);
    exit(1);
  }
}
