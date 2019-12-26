// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange(void *vstart, void *vend);
void orig_kfree(char *v);
extern char end[]; // first address after kernel loaded from ELF file
                   // defined by the kernel linker script in kernel.ld

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  int use_lock;
  struct run *freelist;
} kmem;

// ************** My data structure ****************
struct procFrame {
  int pid;
  struct run *frameNumber;
};

struct procFrame frameList[16384];

static void setFramePID(struct run *num, int pid) {
  for (int i = 0; i < 16384; i++) {
        if (frameList[i].frameNumber == num) {
            frameList[i].pid = pid;
            break;
        }
    }
}

int dump_physmem(int *frames, int *pids, int numframes) {
  if (!frames || !pids || !numframes) {
    return -1;
  }
  if (kmem.use_lock)
      acquire(&kmem.lock);
  int count = 0;
  for (int i = 0; i < 16384; i++) {
    if (frameList[i].pid != 0) {
      frames[count] = (V2P(frameList[i].frameNumber) >> 12) & 0xffff;
      pids[count++] = frameList[i].pid;
    }
    if (count == numframes) {
      break;
    }
  }
  if (kmem.use_lock)
      release(&kmem.lock);
  return 0;
}

static struct run* getNextFreeFrame(int pid) {
  if (frameList[0].pid == 0 && (frameList[1].pid == 0 ||
                                frameList[1].pid == pid)) {
    frameList[0].pid = pid;
    return frameList[0].frameNumber;
  }
  for (int i = 1; i < 16384; i++) {
    if (!frameList[i].pid && pid == -2) {
      frameList[i].pid = pid;
      return frameList[i].frameNumber;
    }
    else if ((frameList[i - 1].pid == pid ||
              frameList[i - 1].pid == 0 ||
              frameList[i - 1].pid == -2) && (frameList[i + 1].pid == pid ||
              frameList[i + 1].pid == 0 ||
              frameList[i + 1].pid == -2) && !frameList[i].pid) {
      frameList[i].pid = pid;
      return frameList[i].frameNumber;
    }
  }
  return 0;
}
// ******************** END ***********************

// Initialization happens in two phases.
// 1. main() calls kinit1() while still using entrypgdir to place just
// the pages mapped by entrypgdir on free list.
// 2. main() calls kinit2() with the rest of the physical pages
// after installing a full page table that maps them on all cores.
void
kinit1(void *vstart, void *vend)
{
  initlock(&kmem.lock, "kmem");
  kmem.use_lock = 0;
  freerange(vstart, vend);
}

void
kinit2(void *vstart, void *vend)
{
  freerange(vstart, vend);
  // *********** ADDED ************
  struct run *num = kmem.freelist;
    for (int i = 0; i < 16384; i++) {
      frameList[i].frameNumber = num;
      frameList[i].pid = 0;
      num = num->next;
    }
  // ************ END *************
  kmem.use_lock = 1;
}

void
freerange(void *vstart, void *vend)
{
  char *p;
  p = (char*)PGROUNDUP((uint)vstart);
  for(; p + PGSIZE <= (char*)vend; p += PGSIZE)
    orig_kfree(p);
}
// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
orig_kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  r = (struct run*)v;
  r->next = kmem.freelist;
  kmem.freelist = r;
  if(kmem.use_lock)
    release(&kmem.lock);
}

void
kfree(char *v)
{
  if((uint)v % PGSIZE || v < end || V2P(v) >= PHYSTOP)
    panic("kfree2");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  if(kmem.use_lock)
    acquire(&kmem.lock);
  // ************ CHANGED *************
  setFramePID((struct run*) v, 0);
  // ************** END ***************
  if(kmem.use_lock)
    release(&kmem.lock);
}


// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  // ********** Changed for v2 ***********
  if (!kmem.use_lock) {
    r = kmem.freelist;
    if (r) {
      kmem.freelist = r->next;
    }
  }
  else {
    r = getNextFreeFrame(-2);
  }
  // **************** END ****************
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

char*
kalloc2(int pid)
{
  struct run *r;

  if(kmem.use_lock)
    acquire(&kmem.lock);
  // ********** Changed for v2 ***********
  if (!kmem.use_lock) {
    r = kmem.freelist;
    if (r) {
      kmem.freelist = r->next;
    }
  }
  else {
    r = getNextFreeFrame(pid);
  }
  // **************** END ****************
  if(kmem.use_lock)
    release(&kmem.lock);
  return (char*)r;
}

