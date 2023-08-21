// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

uint8 pa_ref[(PHYSTOP - KERNBASE) / PGSIZE + 1];
void add_pa_ref(void *pa) {
  pa_ref[(uint64)(pa - KERNBASE) / PGSIZE] += 1;
}
void dec_pa_ref(void *pa) {
  pa_ref[(uint64)(pa - KERNBASE) / PGSIZE] -= 1;
}
int get_pa_ref(void *pa) {
  return pa_ref[(uint64)(pa - KERNBASE) / PGSIZE];
}

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

int free_Pages = 0;
int inited = 0;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  inited = 1;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    kref((void*)p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
int
kfree(void *pa)
{
  // printf("kfree: %p\n", pa);
  // if ((uint64)pa == 0x0000000080519000L)
  //   printf("kfree 0x0000000080519000\n");
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if (get_pa_ref(pa) == 0)
    panic("kfree: ref count is 0");
  
  dec_pa_ref(pa);
  if (get_pa_ref(pa) > 0) {
    release(&kmem.lock);
    return 0;
  }
  if (get_pa_ref(pa) < 0)
    panic("kfree: ref count is negative");
  release(&kmem.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  free_Pages += 1;
  if (get_pa_ref(pa) != 0) {
    printf("panic pa %p\n", pa);
    panic("kfree: ref count is not 0");
  }
  release(&kmem.lock);
  return 1;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r) {
    kmem.freelist = r->next;
    add_pa_ref(r);
    if (get_pa_ref(r) != 1) {
      printf("panic pa %p cnt %d\n", r, get_pa_ref(r));
      panic("kalloc: ref count is not 1");
    }
    free_Pages -= 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  // printf("kalloc: %p\n", r);
  // if ((uint64)r == 0x0000000080519000L)
  //   printf("kalloc 0x0000000080519000\n");
  return (void*)r;
}

void kref(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kref");

  acquire(&kmem.lock);
  if (inited && get_pa_ref(pa) == 0) {
    printf("panic pa %p\n", pa);
    panic("kref: ref count is 0");
  }
  add_pa_ref(pa);
  release(&kmem.lock);
  // printf("kref: %p\n", pa);
}

int dec_ref_if_greater_than_one(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("get_pa_ref");

  int decreased = 0;
  acquire(&kmem.lock);
  if (get_pa_ref(pa) > 1) {
    dec_pa_ref(pa);
    decreased = 1;
  }
  release(&kmem.lock);
  return decreased;
}

void get_free_page_cnt() {
  acquire(&kmem.lock);
  int cnt = free_Pages;
  release(&kmem.lock);
  printf("free pages: %d\n", cnt);
}
