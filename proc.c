#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct {
    struct spinlock lock;
    struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

uint VirtualTime = 0;
uint TotalWeight = 0;
int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

// for test!!!!!!!!
// int printed = 0;

void pinit(void) { initlock(&ptable.lock, "ptable"); }

// Must be called with interrupts disabled
int cpuid() { return mycpu() - cpus; }

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *mycpu(void) {
    int apicid, i;

    if (readeflags() & FL_IF) panic("mycpu called with interrupts enabled\n");

    apicid = lapicid();
    // APIC IDs are not guaranteed to be contiguous. Maybe we should have
    // a reverse map, or reserve a register to store &cpus[i].
    for (i = 0; i < ncpu; ++i) {
        if (cpus[i].apicid == apicid) return &cpus[i];
    }
    panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *myproc(void) {
    struct cpu *c;
    struct proc *p;
    pushcli();
    c = mycpu();
    p = c->proc;
    popcli();
    return p;
}

// PAGEBREAK: 32
//  Look in the process table for an UNUSED proc.
//  If found, change state to EMBRYO and initialize
//  state required to run in the kernel.
//  Otherwise return 0.
static struct proc *allocproc(void) {
    struct proc *p;
    char *sp;

    acquire(&ptable.lock);

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
        if (p->state == UNUSED) goto found;

    release(&ptable.lock);
    return 0;

found:
    p->state = EMBRYO;
    p->pid = nextpid++;

    release(&ptable.lock);

    // Allocate kernel stack.
    if ((p->kstack = kalloc()) == 0) {
        p->state = UNUSED;
        return 0;
    }
    sp = p->kstack + KSTACKSIZE;

    // Leave room for trap frame.
    sp -= sizeof *p->tf;
    p->tf = (struct trapframe *)sp;

    // Set up new context to start executing at forkret,
    // which returns to trapret.
    sp -= 4;
    *(uint *)sp = (uint)trapret;

    sp -= sizeof *p->context;
    p->context = (struct context *)sp;
    memset(p->context, 0, sizeof *p->context);
    p->context->eip = (uint)forkret;

    return p;
}

// PAGEBREAK: 32
//  Set up first user process.
void userinit(void) {
    struct proc *p;
    extern char _binary_initcode_start[], _binary_initcode_size[];

    p = allocproc();

    initproc = p;
    if ((p->pgdir = setupkvm()) == 0) panic("userinit: out of memory?");
    cprintf("%p %p\n", _binary_initcode_start, _binary_initcode_size);
    inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
    p->sz = PGSIZE;
    memset(p->tf, 0, sizeof(*p->tf));
    p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
    p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
    p->tf->es = p->tf->ds;
    p->tf->ss = p->tf->ds;
    p->tf->eflags = FL_IF;
    p->tf->esp = PGSIZE;
    p->tf->eip = 0;  // beginning of initcode.S

    p->request_tick = D_REQ_TICK;  // default request tick for init process
    p->weight = D_WEIGHT;          // default weight for init process
    TotalWeight += D_WEIGHT;       // update total weight

    int curVirtualTime = VirtualTime;
    p->VirtualTimeInit = curVirtualTime;
    p->used_time = 0;  // mathematical reason: all processes are started with lag = 0
    p->used_this_tick = 0;
    p->VirtualEligible = curVirtualTime;  // mathematical reason: used_time is 0
    p->VirtualDeadline = curVirtualTime + (D_REQ_TICK * SCALE / D_WEIGHT);
    p->lag = 0;  // all processes are started with lag = 0

    safestrcpy(p->name, "initcode", sizeof(p->name));
    p->cwd = namei("/");
    p->ticks = 0;

    // this assignment to p->state lets other cores
    // run this process. the acquire forces the above
    // writes to be visible, and the lock is also needed
    // because the assignment might not be atomic.
    acquire(&ptable.lock);

    p->state = RUNNABLE;

    release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n) {
    uint sz;
    struct proc *curproc = myproc();

    sz = curproc->sz;
    if (n > 0) {
        if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0) return -1;
    } else if (n < 0) {
        if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0) return -1;
    }
    curproc->sz = sz;
    switchuvm(curproc);
    return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork(void) {
    int i, pid;
    struct proc *np;
    struct proc *curproc = myproc();

    // Allocate process.
    if ((np = allocproc()) == 0) {
        return -1;
    }

    // Copy process state from proc.
    if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0) {
        kfree(np->kstack);
        np->kstack = 0;
        np->state = UNUSED;
        return -1;
    }
    np->sz = curproc->sz;
    np->parent = curproc;
    int cur_request_tick = curproc->request_tick;
    int cur_weight = curproc->weight;
    np->request_tick = cur_request_tick;  // inherit request tick from parent
    np->weight = cur_weight;              // inherit weight from parent
    TotalWeight += cur_weight;            // update total weight

    int curVirtualTime = VirtualTime;
    np->VirtualTimeInit = curVirtualTime;
    np->used_time = 0;  // mathematical reason: all processes are started with lag = 0
    np->used_this_tick = 0;
    np->VirtualEligible = curVirtualTime;  // mathematical reason: used_time is 0
    np->VirtualDeadline = curVirtualTime + (cur_request_tick * SCALE / cur_weight);
    np->lag = 0;  // all processes are started with lag = 0

    np->ticks = 0;
    *np->tf = *curproc->tf;

    // Clear %eax so that fork returns 0 in the child.
    np->tf->eax = 0;

    for (i = 0; i < NOFILE; i++)
        if (curproc->ofile[i]) np->ofile[i] = filedup(curproc->ofile[i]);
    np->cwd = idup(curproc->cwd);

    safestrcpy(np->name, curproc->name, sizeof(curproc->name));

    pid = np->pid;

    acquire(&ptable.lock);

    np->state = RUNNABLE;

    release(&ptable.lock);

    return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void) {
    struct proc *curproc = myproc();
    struct proc *p;
    int fd;

    if (curproc == initproc) panic("init exiting");

    // Close all open files.
    for (fd = 0; fd < NOFILE; fd++) {
        if (curproc->ofile[fd]) {
            fileclose(curproc->ofile[fd]);
            curproc->ofile[fd] = 0;
        }
    }

    begin_op();
    iput(curproc->cwd);
    end_op();
    curproc->cwd = 0;

    acquire(&ptable.lock);

    // 이 프로세스의 weight를 total weight에서 빼준다.
    TotalWeight -= curproc->weight;

    // Parent might be sleeping in wait().
    wakeup1(curproc->parent);

    // Pass abandoned children to init.
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->parent == curproc) {
            p->parent = initproc;
            if (p->state == ZOMBIE) wakeup1(initproc);
        }
    }

    // Jump into the scheduler, never to return.
    curproc->state = ZOMBIE;
    sched();
    panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void) {
    struct proc *p;
    int havekids, pid;
    struct proc *curproc = myproc();

    acquire(&ptable.lock);
    for (;;) {
        // Scan through table looking for exited children.
        havekids = 0;
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->parent != curproc) continue;
            havekids = 1;
            if (p->state == ZOMBIE) {
                // Found one.
                pid = p->pid;
                kfree(p->kstack);
                p->kstack = 0;
                freevm(p->pgdir);
                p->pid = 0;
                p->parent = 0;
                p->name[0] = 0;
                p->killed = 0;
                p->state = UNUSED;
                release(&ptable.lock);
                return pid;
            }
        }

        // No point waiting if we don't have any children.
        if (!havekids || curproc->killed) {
            release(&ptable.lock);
            return -1;
        }

        // Wait for children to exit.  (See wakeup1 call in proc_exit.)
        sleep(curproc, &ptable.lock);  // DOC: wait-sleep
    }
}

// PAGEBREAK: 42
//  Per-CPU process scheduler.
//  Each CPU calls scheduler() after setting itself up.
//  Scheduler never returns.  It loops, doing:
//   - choose a process to run
//   - swtch to start running that process
//   - eventually that process transfers control
//       via swtch back to the scheduler.
void scheduler(void) {
    struct proc *p, *q;
    struct cpu *c = mycpu();
    c->proc = 0;

    for (;;) {
        // Enable interrupts on this processor.
        sti();

        acquire(&ptable.lock);

        // EEVDF 스케줄러에 필요한 값들을 계산한다.
        for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
            if (p->state == RUNNABLE) {
                p->VirtualEligible = p->VirtualTimeInit + (p->used_time * SCALE / p->weight);
                p->VirtualDeadline = p->VirtualEligible + (p->request_tick * SCALE / p->weight);
                p->lag = p->weight * (VirtualTime - p->VirtualTimeInit) - (p->used_time * SCALE);

                // lag의 계산이 VirtualEligible과 VirtualTime보다 더 정확하기 때문에 comment out하였습니다.
                // if (p->VirtualEligible < VirtualTime) {
                //     // for test!!!!!!!!
                //     if (p->lag < 0) {
                //         cprintf(
                //             "VirtualEligible < VirtualTime but lag < 0. VirtualTime: %d, VirtualEligible: %d, lag: %d\n",
                //             VirtualTime, p->VirtualEligible, p->lag);
                //     }
                //     p->lag = 0;
                // }

                // for test!!!!!!!!
                // cprintf(
                //     "VirtualTime: %d, pid: %d, request_tick: %d, weight: %d, TotalWeight:%d, VirtualTimeInit: "
                //     "%d, "
                //     "used_time: %d, "
                //     "VirtualEligible: %d, "
                //     "VirtualDeadline: %d, lag: %d\n",
                //     VirtualTime, p->pid, p->request_tick, p->weight, TotalWeight, p->VirtualTimeInit, p->used_time,
                //     p->VirtualEligible, p->VirtualDeadline, p->lag);
            }
        }

        // 계산된 값들을 바탕으로 스케줄링을 한다. 실행할 수 있는 프로세스가 있는지 찾는다.
        p = ptable.proc;
        while ((p < &ptable.proc[NPROC]) && !(p->state == RUNNABLE && p->lag >= 0)) {
            p++;
        }

        // while 문을 빠져나온 이유가 p < &ptable.proc[NPROC]를 만족하지 못해서라면 스케줄링을 다시 시작한다.
        if (p >= &ptable.proc[NPROC]) {
            release(&ptable.lock);
            // for test!!!!!!!!
            // if (printed == 0) cprintf("(%d)", VirtualTime);
            // printed = 1;
            continue;
        }

        // 더 나은 프로세스 q가 있는지 찾고, 있다면 이를 p에 저장한다.
        for (q = p + 1; q < &ptable.proc[NPROC]; q++) {
            if ((q->state == RUNNABLE && q->lag >= 0) && (q->VirtualDeadline < p->VirtualDeadline)) {
                p = q;
            } else if ((q->state == RUNNABLE && q->lag >= 0) && (q->VirtualDeadline == p->VirtualDeadline) &&
                       (q->pid < p->pid)) {
                p = q;
            }
        }

        // for test!!!!!!!!
        // cprintf("%d", p->pid);

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        c->proc = p;   // CPU의 proc에 현재 실행할 프로세스 PCB를 저장한다.
        switchuvm(p);  // 유저 가상 메모리를 현재 실행할 프로세스의 메모리로 바꾼다.
        p->state = RUNNING;

        swtch(&(c->scheduler), p->context);  // c->scheduler는 스케줄러의 커널 context를 의미한다.
                                             // p->context는 현재 실행할 프로세스의 커널 context를 의미한다.
        switchkvm();                         // 커널 가상 메모리를 커널의 메모리로 바꾼다.

        if (p->used_this_tick == 0) {
            p->used_time++;
            p->used_this_tick = 1;
        }

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;  // CPU의 proc에 null을 저장한다.

        release(&ptable.lock);
    }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
    int intena;
    struct proc *p = myproc();

    if (!holding(&ptable.lock)) panic("sched ptable.lock");
    if (mycpu()->ncli != 1) panic("sched locks");
    if (p->state == RUNNING) panic("sched running");
    if (readeflags() & FL_IF) panic("sched interruptible");
    intena = mycpu()->intena;
    swtch(&p->context, mycpu()->scheduler);
    mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
    acquire(&ptable.lock);  // DOC: yieldlock
    myproc()->state = RUNNABLE;
    sched();
    release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void) {
    static int first = 1;
    // Still holding ptable.lock from scheduler.
    release(&ptable.lock);

    if (first) {
        // Some initialization functions must be run in the context
        // of a regular process (e.g., they call sleep), and thus cannot
        // be run from main().
        first = 0;
        iinit(ROOTDEV);
        initlog(ROOTDEV);
    }

    // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk) {
    struct proc *p = myproc();

    if (p == 0) panic("sleep");

    if (lk == 0) panic("sleep without lk");

    // Must acquire ptable.lock in order to
    // change p->state and then call sched.
    // Once we hold ptable.lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup runs with ptable.lock locked),
    // so it's okay to release lk.
    if (lk != &ptable.lock) {   // DOC: sleeplock0
        acquire(&ptable.lock);  // DOC: sleeplock1
        release(lk);
    }
    // Go to sleep.
    p->chan = chan;
    p->state = SLEEPING;
    // 주의: 이 프로세스는 더이상 active하지 않으므로 TotalWeight에서 빼야 한다!!!!!!!!
    TotalWeight -= p->weight;

    sched();

    // Tidy up.
    p->chan = 0;

    // Reacquire original lock.
    if (lk != &ptable.lock) {  // DOC: sleeplock2
        release(&ptable.lock);
        acquire(lk);
    }
}

// PAGEBREAK!
//  Wake up all processes sleeping on chan.
//  The ptable lock must be held.
static void wakeup1(void *chan) {
    struct proc *p;

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == SLEEPING && p->chan == chan) {
            p->state = RUNNABLE;
            // 주의: 이 프로세스는 다시 active하므로 TotalWeight에 더해줘야 한다!!!!!!!!
            TotalWeight += p->weight;
        }
    }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan) {
    acquire(&ptable.lock);
    wakeup1(chan);
    release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid) {
    struct proc *p;

    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->pid == pid) {
            p->killed = 1;
            // Wake process from sleep if necessary.
            if (p->state == SLEEPING) p->state = RUNNABLE;
            release(&ptable.lock);
            return 0;
        }
    }
    release(&ptable.lock);
    return -1;
}

// PAGEBREAK: 36
//  Print a process listing to console.  For debugging.
//  Runs when user types ^P on console.
//  No lock to avoid wedging a stuck machine further.
void procdump(void) {
    static char *states[] = {[UNUSED] "unused",   [EMBRYO] "embryo",  [SLEEPING] "sleep ",
                             [RUNNABLE] "runble", [RUNNING] "run   ", [ZOMBIE] "zombie"};
    int i;
    struct proc *p;
    char *state;
    uint pc[10];

    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state == UNUSED) continue;
        if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
            state = states[p->state];
        else
            state = "???";
        cprintf("%d %s %s", p->pid, state, p->name);
        if (p->state == SLEEPING) {
            getcallerpcs((uint *)p->context->ebp + 2, pc);
            for (i = 0; i < 10 && pc[i] != 0; i++) cprintf(" %p", pc[i]);
        }
        cprintf("\n");
    }
}

int csprintf(char *str, int len) {
    int i;
    char tmpChar;
    for (i = 0; str[i] != '\0'; i++) {
        tmpChar = str[i + 1];
        str[i + 1] = '\0';
        cprintf("%s", &(str[i]));
        str[i + 1] = tmpChar;
    }
    for (int j = 0; j < len - i; j++) {
        cprintf(" ");
    }
    return 0;
}

int ciprintf(int num, int len) {
    int i = 0;
    int tmpNum = num;
    if (num < 0) {
        i++;
    }
    do {
        i++;
        tmpNum /= 10;
    } while (tmpNum != 0);

    cprintf("%d", num);
    for (int j = 0; j < len - i; j++) {
        cprintf(" ");
    }
    return 0;
}

int sched_setattr(int request_tick, int weight) {
    struct proc *p;
    p = myproc();

    if (p == 0) {
        return -1;
    }
    int weight_original = p->weight;
    if (weight_original < 1 || weight_original > 5) {
        return -1;
    }

    p->request_tick = request_tick;
    p->weight = weight;

    TotalWeight -= weight_original;
    TotalWeight += weight;

    yield();

    return 0;
}

int sched_getattr(int *request_tickP, int *weightP) {
    struct proc *p;
    p = myproc();

    if (p == 0) {
        return -1;
    }

    if (p->request_tick <= 0) {
        return -1;
    }
    if (p->weight < 1 || p->weight > 5) {
        return -1;
    }

    *request_tickP = p->request_tick;
    *weightP = p->weight;

    return 0;
}

int ps(void) {
    cprintf("name    pid     state   weight  ticks: %d\n", ticks);

    struct proc *p;
    for (int i = 0; i < NPROC; i++) {
        if (ptable.proc[i].state == SLEEPING || ptable.proc[i].state == RUNNABLE || ptable.proc[i].state == RUNNING ||
            ptable.proc[i].state == ZOMBIE) {
            p = &ptable.proc[i];
            char *name = p->name;
            int pid = p->pid;
            int state = p->state;
            int weight = p->weight;
            int ticksPc = p->ticks;

            csprintf(name, 8);
            ciprintf(pid, 8);
            ciprintf(state, 8);
            ciprintf(weight, 8);
            cprintf("%d\n", ticksPc);
        }
    }
    return 0;
}

void update_virtual_time(void) {
    if (TotalWeight > 0) {  // active한 프로세스가 최소 하나 이상 있을 때만 업데이트한다.
        if ((SCALE / TotalWeight) >= 1) {
            VirtualTime += SCALE / TotalWeight;  // max(TotalWeight) = 64 * 5 = 320.
        } else {                                 // in fact, TotalWeight cannot exceed the SCALE.
            VirtualTime += 1;
        }
        // for test!!!!!!!!
        // printed = 0;
    }
}

void update_tick_flag(void) {
    for (struct proc *p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
        if (p->state != UNUSED && p->state != ZOMBIE) {
            p->used_this_tick = 0;  // reset the flag
        }
    }
}