#ifndef SYSMON_H
#define SYSMON_H

#include "types.h"

typedef struct {
    u32 total_ram_kb;
    u32 used_ram_kb;
    u32 free_ram_kb;
    u32 cpu_usage_percent;
    u32 uptime_seconds;
    u32 process_count;
    u32 thread_count;
    u32 disk_total_kb;
    u32 disk_used_kb;
    u32 net_rx_bytes;
    u32 net_tx_bytes;
} sysmon_stats_t;

typedef struct {
    u32 pid;
    char name[32];
    u8  state;
    u32 mem_kb;
    u32 cpu_percent;
} sysmon_process_t;

#define PROC_STATE_RUNNING  0
#define PROC_STATE_SLEEPING 1
#define PROC_STATE_STOPPED  2
#define PROC_STATE_ZOMBIE   3

void sysmon_init(void);
int  sysmon_get_stats(sysmon_stats_t *out);
int  sysmon_get_processes(sysmon_process_t *list, int max_entries);
u32  sysmon_get_uptime(void);
u32  sysmon_get_cpu_usage(void);
u32  sysmon_get_memory_usage(void);
void sysmon_refresh(void);

#endif
