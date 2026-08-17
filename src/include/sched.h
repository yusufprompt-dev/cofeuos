/*
 * CofeuOS - Scheduler API
 *
 * Cooperative/preemptive hybrid scheduler.
 * No malloc - fixed task table.
 */
#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct task task_t;

#define MAX_TASKS 16
#define DEFAULT_TIME_SLICE_MS 10

/* Initialize scheduler */
void sched_init(void);

/* Create a new task
 * Returns task_id (>0) on success, -1 on failure */
int sched_create_task(void (*entry)(void *), void *arg,
                      uint8_t priority, void *stack, size_t stack_size);

/* Yield CPU to next ready task */
void sched_yield(void);

/* Timer tick - call from timer interrupt (delta_ms = ms since last tick) */
void sched_tick(uint32_t delta_ms);

/* Sleep current task for ms milliseconds */
int sched_sleep_ms(uint32_t ms);

/* Get current task ID */
int sched_get_current_id(void);

/* Get system tick count */
uint32_t sched_get_ticks(void);

/* Get number of created tasks */
int sched_get_task_count(void);

/* Get task by index (for debugging) */
struct task *sched_get_task(uint32_t idx);

/* Start the scheduler (call task_wrapper for first task) */
void sched_start(void);

/* Task states */
#define TASK_READY 0
#define TASK_RUNNING 1
#define TASK_BLOCKED 2
#define TASK_SLEEPING 3
#define TASK_ZOMBIE 4

/* Task structure (opaque but visible for debugging) */
struct task {
    uint64_t *stack_base;
    uint64_t *stack_ptr;
    uint32_t stack_size;
    void (*entry)(void *);
    void *arg;
    int state;
    uint8_t priority;
    uint32_t time_slice;
    uint32_t sleep_until;
    uint32_t task_id;
};

#ifdef __cplusplus
}
#endif

#endif /* SCHED_H */