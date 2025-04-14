// test file for priority scheduler

#include "types.h"
#include "stat.h"
#include "user.h"

int dummyCalc();

int main() {
    int pid1, pid2, pid3;
    int parentPid = getpid();
    int dummyRequestTick = 100;

    int weight = 3;
    sched_setattr(dummyRequestTick, weight);
    printf(1, "child1 attribute set by parent. weight: %d\n", weight);
    pid1 = fork();
    if (pid1 == 0) {  // I am child1
        int mypid = getpid();

        int dummyInt = dummyCalc();
        printf(1, "child1 finished, pid: %d, weight: %d, dummyInt: %d\n", mypid, weight, dummyInt);

        exit();
    }

    weight = 4;
    sched_setattr(dummyRequestTick, weight);
    printf(1, "child2 attribute set by parent. weight: %d\n", weight);
    pid2 = fork();
    if (pid2 == 0) {  // I am child2
        int mypid = getpid();

        printf(1, "child2 is now sleeping.\n");
        sleep(200);  // Sleep

        int dummyInt = dummyCalc();
        printf(1, "child2 finished, pid: %d, weight: %d, dummyInt: %d\n", mypid, weight, dummyInt);

        exit();
    }

    sched_setattr(dummyRequestTick, 2);
    printf(1, "Parent is yielding for child1 and child2.\n");
    yield();

    weight = 4;
    sched_setattr(dummyRequestTick, weight);
    printf(1, "child3 attribute set by parent. weight: %d\n", weight);
    pid3 = fork();
    if (pid3 == 0) {  // I am child3
        int mypid = getpid();

        int dummyInt = dummyCalc();
        printf(1, "child3 finished, pid: %d, weight: %d, dummyInt: %d\n", mypid, weight, dummyInt);

        exit();
    }

    printf(1, "Parent waiting start. pid: %d, weight: %d\n", parentPid, 2);
    wait();
    wait();
    wait();

    exit();
}

int dummyCalc() {
    int a = 1;
    int b = 2;
    int c = 0;
    for (int i = 0; i < (__INT32_MAX__ / 4); i++) {
        c = a + b;
        a = b;
        b = c;
    }
    return c;
}