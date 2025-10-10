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
// #define KERNBASE 0x80000000L
// #define PHYSTOP (KERNBASE + 128*1024*1024)
#define PA2RC(pa) (pa-KERNBASE)/PGSIZE
struct {
  struct spinlock lock;
  int p_rc[PA2RC(PHYSTOP)];
} page_rc;
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
  initlock(&page_rc.lock, "page_rc");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  acquire(&page_rc.lock);
  uint64 ppa=PA2RC((uint64)pa);
  page_rc.p_rc[ppa]--;
  if(page_rc.p_rc[ppa]<=0){
  memset(pa, 1, PGSIZE);
  
  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);

  }
  // Fill with junk to catch dangling refs.
  release(&page_rc.lock);
  
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
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    page_rc.p_rc[PA2RC((uint64)r)]=1;}
  return (void*)r;
}
uint64 
kopy_page(uint64 pa)   
{
  int n=PA2RC(pa);
  acquire(&page_rc.lock);
  if(page_rc.p_rc[n]<=1){
    release(&page_rc.lock);
    return pa;
  }
  uint64 newpa=(uint64)kalloc();
  if(newpa==0){
    release(&page_rc.lock);
    return 0;
  }
  memmove((void*)newpa, (void*)pa, PGSIZE);
  page_rc.p_rc[(PA2RC(pa))]--;
  release(&page_rc.lock);
  
  return newpa;
}
void change_rc(uint64 pa)
{
  acquire(&page_rc.lock);
  page_rc.p_rc[PA2RC(pa)]++;
  release(&page_rc.lock);
}