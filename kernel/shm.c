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
  if (shm_table.page != 0)
  {
    return 0;
  }
  shm_table.page = kalloc();
  if (shm_table.page == 0)
  {
    return -1;
  }
  memset(shm_table.page, 0, PGSIZE);
  shm_table.refcnt = 0;
  return 0;
}

uint64
sys_shm_attach(void)
{
  struct proc *p = myproc();
  uint64 pa;

  if (shm_table.page == 0)
  {
    return -1;
  }

  pa = (uint64)shm_table.page;
  pte_t *pte = walk(p->pagetable, SHMEM, 0);
  if (pte && (*pte & PTE_V))
  {
    return -1;
  }

  if (mappages(p->pagetable, SHMEM, PGSIZE, pa, PTE_R | PTE_W | PTE_U) < 0)
  {
    return -1;
  }

  shm_table.refcnt++;

  return SHMEM;
}

uint64
sys_shm_detach(void)
{
  struct proc *p = myproc();
  pte_t *pte = walk(p->pagetable, SHMEM, 0);
  if (pte == 0 || (*pte & PTE_V) == 0)
  {
    return -1;
  }
  uvmunmap(p->pagetable, SHMEM, 1, 0);

  if (shm_table.refcnt > 0)
    shm_table.refcnt--;
  return 0;
}

uint64
sys_shm_destroy(void)
{
  if (shm_table.page == 0)
  {
    return -1;
  }

  if (shm_table.refcnt > 0)
  {
    return -1;
  }
  kfree(shm_table.page);
  shm_table.page = 0;
  shm_table.refcnt = 0;
  return 0;
}

uint64
sys_shm_refcnt(void)
{
  int cnt;
  cnt = shm_table.refcnt;
  return cnt;
}
