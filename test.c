// test file for sched_setattr and sched_getattr

#include "types.h"
#include "stat.h"
#include "user.h"

int main() {
    printf(1, "sched_setattr(), return value: %d\n", sched_setattr(10, 10));

    int* request_tickP = malloc(sizeof(int));
    int* weightP = malloc(sizeof(int));
    printf(1, "sched_getattr(), return value: %d\n", sched_getattr(request_tickP, weightP));
    printf(1, "request_tick: %d, weight: %d\n", *request_tickP, *weightP);

    return 0;
}
