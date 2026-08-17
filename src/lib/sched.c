/*
 * CofeuOS - Multitasking Scheduler
 *
 * Cooperative/preemptive hybrid scheduler with:
 * - Fixed priority levels (0=highest)
 * - Round-robin within same priority
 * - Time-slice preemption (configurable)
 * - Stack isolation per task
 * - No malloc - fixed task table
 */
#include <stdint.h>
#include <stddef.h>
#include "../include/string.h"
#include "../include/sched.h"

#define MAX_TASKS 16
#define DEFAULT_TIME_SLICE_MS 10
#define STACK_SIZE 4096

static struct task tasks[MAX_TASKS];
static uint32_t task_count = 0;
static uint32_t current_task = 0;
static uint32_t next_task_id = 1;
static uint32_t system_ticks = 0;
static uint32_t scheduler_lock = 0;

#define SCHED_LOCK()   do { while (__sync_lock_test_and_set(&scheduler_lock, 1)) ; } while(0)
#define SCHED_UNLOCK() __sync_lock_release(&scheduler_lock)

static inline void context_save(uint64_t **sp) {
    __asm__ volatile (
        "push %%rax\n"
        "push %%rbx\n"
        "push %%rcx\n"
        "push %%rdx\n"
        "push %%rsi\n"
        "push %%rdi\n"
        "push %%rbp\n"
        "push %%r8\n"
        "push %%r9\n"
        "push %%r10\n"
        "push %%r11\n"
        "push %%r12\n"
        "push %%r13\n"
        "push %%r14\n"
        "push %%r15\n"
        "mov %%rsp, %0\n"
        : "=r" (*sp)
        :
        : "memory", "cc"
    );
}

static inline void context_restore(uint64_t *sp) {
    __asm__ volatile (
        "mov %0, %%rsp\n"
        "pop %%r15\n"
        "pop %%r14\n"
        "pop %%r13\n"
        "pop %%r12\n"
        "pop %%r11\n"
        "pop %%r10\n"
        "pop %%r9\n"
        "pop %%r8\n"
        "pop %%rbp\n"
        "pop %%rdi\n"
        "pop %%rsi\n"
        "pop %%rdx\n"
        "pop %%rcx\n"
        "pop %%rbx\n"
        "pop %%rax\n"
        :
        : "r" (sp)
        : "memory", "cc"
    );
}

static void task_wrapper(void) {
    struct task *t = (struct task *)&tasks[current_task];
    t->entry(t->arg);
    SCHED_LOCK();
    t->state = TASK_ZOMBIE;
    SCHED_UNLOCK();
    sched_yield(); /* Never returns */
}

/* Public function to start the first task */
void sched_start(void) {
    SCHED_LOCK();
    if (task_count > 0) {
        current_task = 0;
        tasks[0].state = TASK_RUNNING;
        SCHED_UNLOCK();
        task_wrapper(); /* Never returns */
    }
    SCHED_UNLOCK();
}

void sched_init(void) {
    memset(tasks, 0, sizeof(tasks));
    task_count = 0;
    current_task = 0;
    next_task_id = 1;
    system_ticks = 0;
    scheduler_lock = 0;
}

int sched_create_task(void (*entry)(void *), void *arg, uint8_t priority, void *stack, size_t stack_size) {
    if (task_count >= MAX_TASKS) return -1;
    if (stack_size < 1024) return -1;

    SCHED_LOCK();

    uint32_t tid = next_task_id++;
    struct task *t = &tasks[task_count];
    t->task_id = tid;
    t->entry = entry;
    t->arg = arg;
    t->priority = priority;
    t->state = TASK_READY;
    t->time_slice = DEFAULT_TIME_SLICE_MS;
    t->sleep_until = 0;
    t->stack_base = (uint64_t *)((uintptr_t)stack + stack_size);
    t->stack_ptr = t->stack_base;
    t->stack_size = stack_size;

    /* Setup initial stack frame for context_restore */
    /* x86_64 context_restore expects: rax, rbx, rcx, rdx, rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15, rip */
    uint64_t *sp = (uint64_t *)((uintptr_t)t->stack_base - 16 * sizeof(uint64_t));
    memset(sp, 0, 16 * sizeof(uint64_t));
    /* Set RIP (position 15) to task_wrapper */
    sp[15] = (uint64_t)(uintptr_t)task_wrapper;

    t->stack_ptr = sp;
    task_count++;

    SCHED_UNLOCK();
    return (int)tid;
}

static int sched_find_next_task(void) {
    /* Round-robin within same priority */
    uint8_t best_prio = 255;
    int best_idx = -1;

    for (uint32_t i = 0; i < task_count; i++) {
        struct task *t = (struct task *)&tasks[i];
        if (t->state != TASK_READY) continue;
        if (t->priority < best_prio) {
            best_prio = t->priority;
            best_idx = i;
        }
    }

    return best_idx; /* -1 if none ready */
}

void sched_yield(void) {
    SCHED_LOCK();

    struct task *cur = (struct task *)&tasks[current_task];
    if (cur->state == TASK_RUNNING) {
        context_save(&cur->stack_ptr);
        cur->state = TASK_READY;
    }

    int next = sched_find_next_task();
    if (next >= 0 && next != current_task) {
        current_task = next;
        tasks[current_task].state = TASK_RUNNING;
        context_restore(tasks[current_task].stack_ptr);
    } else if (tasks[current_task].state == TASK_READY) {
        /* First time starting this task - call task_wrapper directly */
        tasks[current_task].state = TASK_RUNNING;
        SCHED_UNLOCK();
        task_wrapper(); /* Never returns */
    } else if (cur->state == TASK_RUNNING) {
        /* No other ready tasks, continue current */
        context_restore(cur->stack_ptr);
    } else {
        /* No ready tasks, return to caller */
    }

    SCHED_UNLOCK();
}

void sched_tick(uint32_t delta_ms) {
    SCHED_LOCK();
    system_ticks += delta_ms;

    /* Update sleeping tasks */
    for (uint32_t i = 0; i < task_count; i++) {
        struct task *t = (struct task *)&tasks[i];
        if (t->state == TASK_SLEEPING && t->sleep_until <= system_ticks) {
            t->state = TASK_READY;
            t->sleep_until = 0;
        }
    }

    /* Time slice for current task */
    struct task *cur = (struct task *)&tasks[current_task];
    if (cur->state == TASK_RUNNING) {
        if (cur->time_slice <= delta_ms) {
            cur->time_slice = DEFAULT_TIME_SLICE_MS;
            SCHED_UNLOCK();
            sched_yield(); /* Re-locks internally */
            return;
        } else {
            cur->time_slice -= delta_ms;
        }
    }

    SCHED_UNLOCK();
}

int sched_sleep_ms(uint32_t ms) {
    SCHED_LOCK();
    struct task *cur = (struct task *)&tasks[current_task];
    if (cur->state != TASK_RUNNING) {
        SCHED_UNLOCK();
        return -1;
    }
    cur->state = TASK_SLEEPING;
    cur->sleep_until = system_ticks + ms;
    context_save(&cur->stack_ptr);
    int next = sched_find_next_task();
    if (next != current_task) {
        current_task = next;
        tasks[current_task].state = TASK_RUNNING;
        context_restore(tasks[current_task].stack_ptr);
    } else {
        context_restore(cur->stack_ptr);
    }
    SCHED_UNLOCK();
    return 0;
}

int sched_get_current_id(void) {
    return tasks[current_task].task_id;
}

uint32_t sched_get_ticks(void) {
    return system_ticks;
}

int sched_get_task_count(void) {
    return task_count;
}

struct task *sched_get_task(uint32_t idx) {
    if (idx >= task_count) return NULL;
    return (struct task *)&tasks[idx];
}