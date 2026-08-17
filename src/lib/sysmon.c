#include "../include/sysmon.h"
#include "../include/memory.h"
#include "../include/sched.h"
#include "../include/time.h"

extern memory_arena g_mem_arena;

static sysmon_stats_t g_stats;
static sysmon_process_t g_procs[32];
static int g_proc_count = 0;

static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0';
}

void sysmon_init(void) {
    g_proc_count = 0;
}

int sysmon_get_stats(sysmon_stats_t *out) {
    if (!out) return -1;

    /* Gercek bellek: arena'dan */
    size_t total = g_mem_arena.total_size;
    size_t used  = mem_used_space(&g_mem_arena);

    out->total_ram_kb = (u32)(total / 1024);
    out->used_ram_kb  = (u32)(used / 1024);
    out->free_ram_kb  = (u32)((total - used) / 1024);

    /* CPU: hesaplanamaz (UEFI), gercek is sayisini gostericisi olarak kullan */
    out->cpu_usage_percent = 0;

    /* Uptime: scheduler tick'indan */
    out->uptime_seconds = sched_get_ticks() / 1000;

    /* Process/thread: scheduler'dan */
    out->process_count = sched_get_task_count();
    out->thread_count  = sched_get_task_count(); /* cooperatif, thread yok */

    /* Disk: RAM-based FS boyutu */
    out->disk_total_kb = 256;  /* 256 KB VFS kapasitesi */
    out->disk_used_kb  = 0;    /* Gercek kullanilamaz */

    /* Ag: sifirla (gercek trafik bilgisi yok) */
    out->net_rx_bytes = 0;
    out->net_tx_bytes = 0;

    return 0;
}

int sysmon_get_processes(sysmon_process_t *list, int max_entries) {
    if (!list || max_entries <= 0) return 0;

    int written = 0;

    /* 1. Kernel */
    if (written < max_entries) {
        str_copy(list[written].name, "kernel", 32);
        list[written].pid = 1;
        list[written].state = PROC_STATE_RUNNING;
        list[written].mem_kb = (u32)(mem_used_space(&g_mem_arena) / 1024);
        list[written].cpu_percent = 0;
        written++;
    }

    /* 2. Shell (ana dongu) */
    if (written < max_entries) {
        str_copy(list[written].name, "shell", 32);
        list[written].pid = 2;
        list[written].state = PROC_STATE_RUNNING;
        list[written].mem_kb = 64;
        list[written].cpu_percent = 0;
        written++;
    }

    g_proc_count = written;
    return written;
}

u32 sysmon_get_uptime(void) { return sched_get_ticks() / 1000; }
u32 sysmon_get_cpu_usage(void) { return 0; }
u32 sysmon_get_memory_usage(void) { return (u32)(mem_used_space(&g_mem_arena) / 1024); }

void sysmon_refresh(void) {
    /* Gercek veri: bir sey yapmaya gerek yok, get_stats her cagrida guncel */
}
