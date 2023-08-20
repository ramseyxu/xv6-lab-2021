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

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

int pa_ref[(PHYSTOP - KERNBASE) / PGSIZE + 1];
int free_Pages = 0;

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
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&kmem.lock);
  if (pa_ref[(uint64)pa / PGSIZE] == 0)
    panic("kfree: ref count is 0");
  
  pa_ref[(uint64)pa / PGSIZE] -= 1;
  if (pa_ref[(uint64)pa / PGSIZE] > 0) {
    release(&kmem.lock);
    return 0;
  }
  if (pa_ref[(uint64)pa / PGSIZE] < 0)
    panic("kfree: ref count is negative");
  release(&kmem.lock);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  free_Pages += 1;
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
    pa_ref[(uint64)r / PGSIZE] = 1;
    free_Pages -= 1;
  }
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  // printf("kalloc: %p\n", r);
  return (void*)r;
}

void kref(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kref");

  acquire(&kmem.lock);
  pa_ref[(uint64)pa / PGSIZE] += 1;
  release(&kmem.lock);
  // printf("kref: %p\n", pa);
}

int dec_ref_if_greater_than_one(void *pa) {
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("get_pa_ref");

  int decreased = 0;
  acquire(&kmem.lock);
  if (pa_ref[(uint64)pa / PGSIZE] > 1) {
    pa_ref[(uint64)pa / PGSIZE] -= 1;
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
