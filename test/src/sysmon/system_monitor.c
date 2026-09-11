#include <log.h>
#include "system_monitor.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "pin.h"

#define SYSTEM_MONITOR_TASK_PRIORITY  (configMAX_PRIORITIES - 2U)
#define SYSTEM_MONITOR_TASK_STACK_SIZE configMINIMAL_STACK_SIZE
#define SYSTEM_MONITOR_PERIOD_MS       1000U

extern char _heap_start[];
extern char _heap_end[];
extern char *_sbrk(int incr);

static TaskHandle_t system_monitor_task_handle = NULL;
heap_stats_t g_heap_stats = {0};

void vUpdateHeapStats(void)
{
    // xPortGetFreeHeapSize 返回的是当前最大可用连续块，不是所有空闲块之和
    size_t current_free = xPortGetFreeHeapSize();
    g_heap_stats.free_heap_size = current_free;
    g_heap_stats.used_heap_size = g_heap_stats.total_heap_size - current_free;
    // 记录历史最小值
    if (current_free < g_heap_stats.minimum_free_ever)
    {
        g_heap_stats.minimum_free_ever = current_free;
    }
}

// 初始化函数
void vInitializeHeapStats(void)
{
    memset((void *)&g_heap_stats, 0, sizeof(heap_stats_t));
    g_heap_stats.total_heap_size = configTOTAL_HEAP_SIZE;
    g_heap_stats.minimum_free_ever = configTOTAL_HEAP_SIZE;
    vUpdateHeapStats(); // 初始化后进行一次更新
}


// Initialize C heap stats
void vInitializeCHeapStats(void)
{
    g_heap_stats.c_heap_used = 0;
    g_heap_stats.c_heap_total = 0;
}

size_t get_heap_used(void)
{
    static u8 *heap = NULL;
    u8 *prev_heap;
    static u8 *HeapEndPtr = (u8 *)&_heap_end;
    char *Status;

    if (heap == NULL)
    {
        heap = (u8 *)&_heap_start;
    }
    prev_heap = heap;

    if (((heap + 0) <= HeapEndPtr) && (prev_heap != NULL))
    {
        heap += 0;
        Status = (char *)((void *)prev_heap);
    }
    else
    {
        Status = (char *)-1;
    }

    return Status - _heap_start;
}

size_t get_heap_free(void)
{
    return _heap_end - (char *)_sbrk(0);
}


// Update C heap stats using sbrk
void vUpdateCHeapStats(void)
{
	g_heap_stats.c_heap_used = get_heap_used();
	g_heap_stats.c_heap_total = _heap_end - _heap_start; // This is the current heap size
}


static void system_monitor_task(void *arg)
{
    TickType_t last_wake_time;
    vInitializeHeapStats();
    (void)arg;

    last_wake_time = xTaskGetTickCount();
    pin_output(LED_PIN);
    for (;;)
    {
        vTaskDelayUntil(&last_wake_time,
                        pdMS_TO_TICKS(SYSTEM_MONITOR_PERIOD_MS));
        pin_write(LED_PIN, pin_read(LED_PIN) ^ 1);
        vUpdateHeapStats();
    }
}

int system_monitor_start(void)
{
    if (system_monitor_task_handle != NULL)
    {
        return 0;
    }

    if (xTaskCreate(system_monitor_task,
                    "system_monitor",
                    SYSTEM_MONITOR_TASK_STACK_SIZE,
                    NULL,
                    SYSTEM_MONITOR_TASK_PRIORITY,
                    &system_monitor_task_handle) != pdPASS)
    {
        system_monitor_task_handle = NULL;
        return -1;
    }

    return 0;
}

void vApplicationMallocFailedHook(void)
{
    printf("malloc failed\n");
}

void vApplicationIdleHook(void)
{
    /* The system monitor task owns watchdog servicing. */
}

