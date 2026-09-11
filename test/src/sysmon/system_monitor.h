#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H


typedef struct
{
    size_t total_heap_size;
    size_t free_heap_size;
    size_t minimum_free_ever;
    size_t used_heap_size;
    size_t c_heap_total;
    size_t c_heap_used;
} heap_stats_t;
// 全局内存统计结构体
extern heap_stats_t g_heap_stats;

int system_monitor_start(void);

#endif
