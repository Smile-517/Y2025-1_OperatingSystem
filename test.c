// test file for sched_setattr and sched_getattr

#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
    int* request_tickPB = malloc(sizeof(int));
    int* weightPB = malloc(sizeof(int));
    printf(1, "sched_getattr() before calling sched_setattr(), return value: %d\n",
           sched_getattr(request_tickPB, weightPB));
    printf(1, "request_tick: %d, weight: %d\n\n", *request_tickPB, *weightPB);

    printf(1, "first sched_setattr(), return value: %d\n", sched_setattr(100, 3));
    int* request_tickP = malloc(sizeof(int));
    int* weightP = malloc(sizeof(int));
    printf(1, "sched_getattr() after calling sched_setattr(), return value: %d\n",
           sched_getattr(request_tickP, weightP));
    printf(1, "request_tick: %d, weight: %d\n\n", *request_tickP, *weightP);

    int pid;
    int* request_tickP2 = malloc(sizeof(int));
    int* weightP2 = malloc(sizeof(int));
    if ((pid = fork()) == 0) {  // I am child
        printf(1, "child 1\n");
        // yield();
        printf(1, "child 2\n");
        printf(1, "sched_getattr() at child, return value: %d\n", sched_getattr(request_tickP2, weightP2));
        printf(1, "request_tick: %d, weight: %d\n", *request_tickP2, *weightP2);
        printf(1, "child finished\n");
    } else {  // I am parent
        printf(1, "parent 1\n");
        // yield();
        printf(1, "parent 2\n");
        wait();
        printf(1, "parent finished\n");
    }

    exit();
    return 0;  // not reachable
}
