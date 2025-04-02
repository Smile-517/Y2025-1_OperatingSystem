#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sys_fork(void) { return fork(); }

int sys_exit(void) {
    exit();
    return 0;  // not reached
}

int sys_wait(void) { return wait(); }

int sys_kill(void) {
    int pid;

    if (argint(0, &pid) < 0) return -1;
    return kill(pid);
}

int sys_getpid(void) { return myproc()->pid; }

int sys_sbrk(void) {
    int addr;
    int n;

    if (argint(0, &n) < 0) return -1;
    addr = myproc()->sz;
    if (growproc(n) < 0) return -1;
    return addr;
}

int sys_sleep(void) {
    int n;
    uint ticks0;

    if (argint(0, &n) < 0) return -1;
    acquire(&tickslock);
    ticks0 = ticks;
    while (ticks - ticks0 < n) {
        if (myproc()->killed) {
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
int sys_uptime(void) {
    uint xticks;

    acquire(&tickslock);
    xticks = ticks;
    release(&tickslock);
    return xticks;
}

// implement new syscalls
int sys_sched_setattr(void) {
    int request_tick;
    int weight;

    argint(0, &request_tick);
    argint(1, &weight);

    if (request_tick <= 0) {
        return -1;
    }
    myproc()->request_tick = request_tick;

    if (weight < 1) {
        weight = 1;
    } else if (weight > 5) {
        weight = 5;
    }

    myproc()->weight = weight;
    return 0;
}

int sys_sched_getattr(void) {
    int* request_tickP;
    int* weightP;

    argptr(0, (char**)&request_tickP, sizeof(int));
    argptr(1, (char**)&weightP, sizeof(int));

    if (myproc()->request_tick <= 0) {
        return -1;
    }
    if (myproc()->weight < 1 || myproc()->weight > 5) {
        return -1;
    }
    *request_tickP = myproc()->request_tick;
    *weightP = myproc()->weight;
    return 0;
}

int sys_yield(void) {
    yield();
    return 0;
}