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

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

#define INIT_FREELISTS 4

static int
try_acquire(struct spinlock *lk)
{
  if(__sync_lock_test_and_set(&lk->locked, 1) != 0)
    return 0;

  __sync_synchronize();
  lk->cpu = mycpu();
  return 1;
}

static void
release_without_pop(struct spinlock *lk)
{
  lk->cpu = 0;
  __sync_synchronize();
  __sync_lock_release(&lk->locked);
}

static struct run*
steal_from(int victim)
{
  struct run *list;

  list = kmem[victim].freelist;
  kmem[victim].freelist = 0;
  return list;
}

void
kinit()
{
  for(int i = 0; i < NCPU; i++)
    initlock(&kmem[i].lock, "kmem");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  struct run *r;
  int id = 0;

  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE){
    memset(p, 1, PGSIZE);
    r = (struct run*)p;

    acquire(&kmem[id].lock);
    r->next = kmem[id].freelist;
    kmem[id].freelist = r;
    release(&kmem[id].lock);

    id = (id + 1) % INIT_FREELISTS;
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  int id;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  push_off();
  id = cpuid();
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  struct run *list;
  int id;
  int busy;
  int empty_rounds;

  push_off();
  id = cpuid();

  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r)
    kmem[id].freelist = r->next;
  release(&kmem[id].lock);

  empty_rounds = 0;
  while(r == 0 && empty_rounds < 1000){
    busy = 0;
    for(int i = 1; i < NCPU; i++){
      int victim = (id + i) % NCPU;

      if(!try_acquire(&kmem[victim].lock)){
        busy = 1;
        continue;
      }
      list = steal_from(victim);
      release_without_pop(&kmem[victim].lock);

      if(list){
        r = list;
        list = r->next;
        r->next = 0;

        if(list){
          acquire(&kmem[id].lock);
          kmem[id].freelist = list;
          release(&kmem[id].lock);
        }
        break;
      }
    }

    if(busy)
      empty_rounds = 0;
    else
      empty_rounds++;
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
