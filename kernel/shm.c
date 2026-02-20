#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "shm.h"

#define SHMEM (TRAPFRAME - PGSIZE)

struct
{
  struct spinlock lock;
  void *page;
  int refcnt;
} shm_table;

void shm_init_system(void)
{
  initlock(&shm_table.lock, "shm");
  shm_table.page = 0;
  shm_table.refcnt = 0;
}

uint64
sys_shm_init(void)
{
  acquire(&shm_table.lock);
  if (shm_table.refcnt >= 0)
  {
    release(&shm_table.lock);
    return -1; // Already initialized
  }
  shm_table.page = kalloc();
  if (shm_table.page == 0)
  {
    release(&shm_table.lock);
    return -1;
  }
  memset(shm_table.page, 0, PGSIZE);
  shm_table.refcnt = 0;
  release(&shm_table.lock);
  return 0;
}

uint64
sys_shm_attach(void)
{
  struct proc *p = myproc();
  uint64 pa;

  acquire(&shm_table.lock);
  if (shm_table.page == 0)
  {
    release(&shm_table.lock);
    return -1;
  }

  pa = (uint64)shm_table.page;
  pte_t *pte = walk(p->pagetable, SHMEM, 0);
  if (pte && (*pte & PTE_V))
  {
    release(&shm_table.lock);
    return -1; // Already attached
  }

  if (mappages(p->pagetable, SHMEM, PGSIZE, pa, PTE_R | PTE_W | PTE_U) < 0)
  {
    release(&shm_table.lock);
    return -1;
  }

  shm_table.refcnt++;
  release(&shm_table.lock);

  return SHMEM; // Return virtual address
}

uint64
sys_shm_detach(void)
{
  struct proc *p = myproc();

  acquire(&shm_table.lock);

  // Refcount decrement is tricky if we don't know if this process actually attached it.
  // But lab instructions imply simple refcount handling.
  // Typically detach unmaps.

  pte_t *pte = walk(p->pagetable, SHMEM, 0);
  if (pte == 0 || (*pte & PTE_V) == 0)
  {
    release(&shm_table.lock);
    return -1; // Not mapped
  }

  // uvmunmap(pagetable, va, npages, do_free)
  // do_free should be 0 because the physical page is managed by shm_table
  uvmunmap(p->pagetable, SHMEM, 1, 0);

  if (shm_table.refcnt > 0)
    shm_table.refcnt--;

  release(&shm_table.lock);
  return 0;
}

uint64
sys_shm_destroy(void)
{
  acquire(&shm_table.lock);
  if (shm_table.page == 0)
  {
    release(&shm_table.lock);
    return -1;
  }

  if (shm_table.refcnt > 0)
  {
    release(&shm_table.lock);
    return -1; // Can't destroy if in use
  }

  kfree(shm_table.page);
  shm_table.page = 0;
  shm_table.refcnt = 0;

  release(&shm_table.lock);
  return 0;
}

uint64
sys_shm_refcnt(void)
{
  int cnt;
  acquire(&shm_table.lock);
  cnt = shm_table.refcnt;
  release(&shm_table.lock);
  return cnt;
}
