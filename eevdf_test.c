#include "types.h"
#include "stat.h"
#include "user.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf(1, "Usage: eevdf_test <mode>\n");
        printf(1, "mode 0: original\n");
        printf(1, "mode 1: change weight\n");
        printf(1, "mode 2: no sched_setattr\n");
        exit();
    }
    const int mode = atoi(argv[1]);

    if (mode == 0) {  // original (code from reference)
        int iterations = __INT32_MAX__ / 2;
        int bench_start = uptime();

        int pid1 = fork();
        if (pid1 == 0) {
            sched_setattr(5, 1);  //"If you have implemented it, remove the comments."
            // int myPid1 = getpid();
            int start = uptime();
            // printf(1, "\n2 start. pid: %d, weight: %d\n", myPid1, 1);
            for (volatile int i = 0; i < iterations; i++) {
            }  // low weight
            int end = uptime();
            printf(1, "2 Time: %d\n", end - start);
            exit();
        }
        int pid2 = fork();
        if (pid2 == 0) {
            sched_setattr(5, 5);  //"If you have implemented it, remove the comments."
            // int myPid2 = getpid();
            int start = uptime();
            // printf(1, "\n1 start. pid: %d, weight: %d\n", myPid2, 5);
            for (volatile int i = 0; i < iterations; i++) {
            }  // high weight
            int end = uptime();
            printf(1, "1 Time: %d\n", end - start);
            exit();
        }

        wait();
        wait();
        int bench_end = uptime();
        printf(1, "[FINISH] Time: %d\n", bench_end - bench_start);
        exit();
    } else if (mode == 1) {
        int iterations = __INT32_MAX__ / 4;
        int bench_start = uptime();
        int request_tick = 5;

        int pid1 = fork();
        if (pid1 == 0) {
            sched_setattr(request_tick, 1);  // "If you have implemented it, remove the comments."
            int myPid1 = getpid();
            int start = uptime();
            printf(1, "\npid: %d start. weight: %d\n", myPid1, 1);
            for (volatile int i = 0; i < iterations; i++) {
            }
            int end = uptime();
            printf(1, "\npid: %d, Time: %d\n", myPid1, end - start);
            exit();
        }
        int pid2 = fork();
        if (pid2 == 0) {
            sched_setattr(request_tick, 5);  // "If you have implemented it, remove the comments."
            int myPid2 = getpid();
            int start = uptime();
            printf(1, "\npid: %d start. weight: %d\n", myPid2, 5);
            for (volatile int i = 0; i < iterations / 2; i++) {
            }
            sched_setattr(request_tick, 1);  // weight 를 다시 내려놓아 pid1과 같게 한다.
            printf(1, "\nweight of pid: %d is changed to 1\n", myPid2);
            for (volatile int i = 0; i < iterations / 2; i++) {
            }
            int end = uptime();
            printf(1, "\npid: %d, Time: %d\n", myPid2, end - start);
            exit();
        }
        int pid3 = fork();
        if (pid3 == 0) {
            sched_setattr(request_tick, 3);  // "If you have implemented it, remove the comments."
            int myPid3 = getpid();
            int start = uptime();
            printf(1, "\npid: %d start. weight: %d\n", myPid3, 3);
            for (volatile int i = 0; i < iterations; i++) {
            }
            int end = uptime();
            printf(1, "\npid: %d, Time: %d\n", myPid3, end - start);
            exit();
        }

        wait();
        wait();
        wait();
        int bench_end = uptime();
        printf(1, "\n[FINISH] Time: %d\n", bench_end - bench_start);
        exit();
    } else if (mode == 2) {  // no sched_setattr
        int iterations = __INT32_MAX__ / 2;
        int bench_start = uptime();

        int pid1 = fork();
        if (pid1 == 0) {
            int myPid1 = getpid();
            int start = uptime();
            printf(1, "\n1 start. pid: %d, weight: %d\n", myPid1, 1);
            for (volatile int i = 0; i < iterations; i++) {
            }
            int end = uptime();
            printf(1, "\n1 Time: %d\n", end - start);
            exit();
        }
        int pid2 = fork();
        if (pid2 == 0) {
            int myPid2 = getpid();
            int start = uptime();
            printf(1, "\n2 start. pid: %d, weight: %d\n", myPid2, 1);
            for (volatile int i = 0; i < iterations; i++) {
            }
            int end = uptime();
            printf(1, "\n2 Time: %d\n", end - start);
            exit();
        }

        wait();
        wait();
        int bench_end = uptime();
        printf(1, "[FINISH] Time: %d\n", bench_end - bench_start);
        exit();
    }
}
