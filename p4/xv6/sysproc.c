#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// ************* My system calls *************

// Sets the priority of the specified PID to pri
int
sys_setpri(void) {
    int PID;
    int pri;

    if (argint(0, &PID) < 0 || argint(1, &pri) < 0) {
        return -1;
    }

    return setpri(PID, pri);
}

// Returns the current priority of the specified PID
int
sys_getpri(void) {
    int PID;

    if (argint(0, &PID) < 0){
        return -1;
    }

    return getpri(PID);
}

// Similar to fork, but new child is set to the specified priority
int
sys_fork2(void) {
  int pri;

  if (argint(0, &pri) < 0) {
    return -1;
  }

  return fork2(pri);
}

// Extracts pstat struct info
int
sys_getpinfo(void) {
    struct pstat *ptr;

    if (argptr(0, (char**)&ptr, sizeof(ptr)) < 0){
        return -1;
    }

    return getpinfo(ptr);
}
