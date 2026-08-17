/*
 * CofeuOS Scheduler Test - Direct task_wrapper test
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../include/sched.h"
#include "../include/string.h"

#define STACK_SIZE 8192
static uint8_t stack1[8192];

volatile int task1_count = 0;
volatile int done = 0;

void task1(void *arg) {
    (void)arg;
    for (int i = 0; i < 5; i++) {
        task1_count++;
        printf("task1: %d\n", task1_count);
    }
    done = 1;
    printf("task1 done\n");
}

int main(void) {
    printf("Direct task_wrapper test...\n");
    sched_init();

    int id1 = sched_create_task(task1, NULL, 1, stack1, sizeof(stack1));

    if (id1 <= 0) {
        printf("FAIL: task creation\n");
        return 1;
    }

    printf("Task created: %d\n", id1);

    /* Call sched_start to test execution */
    printf("Calling sched_start...\n");
    sched_start();

    printf("Back from sched_start\n");
    
    printf("task1_count=%d, done=%d\n", task1_count, done);

    if (task1_count == 5 && done == 1) {
        printf("SONUC: OK\n");
        return 0;
    } else {
        printf("SONUC: FAIL\n");
        return 1;
    }
}