/*
 * Copyright (C) 2016 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "FreeRTOS.h"
#include "task.h"
#include "time.h"
#include <sys/time.h>
#include "shell.h"
#include "sntp.h"
#include "system_monitor.h"

static int free_cmd(int argc, char **argv)
{
    printf("Memory info:\n");
    printf("Total heap: %dB\n", g_heap_stats.total_heap_size);
    printf("Free heap: %dB\n", g_heap_stats.free_heap_size);
    printf("Minimum free ever: %dB\n", g_heap_stats.minimum_free_ever);
    printf("Used heap: %dB\n", g_heap_stats.used_heap_size);
    printf("C heap total: %dB\n", g_heap_stats.c_heap_total);
    printf("C heap used: %dB\n", g_heap_stats.c_heap_used);
    return 0;
}

void register_free_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = "free",
        .help = "Print memory info",
        .hint = NULL,
        .func = &free_cmd,
    };
    esp_console_cmd_register(&cmd);
}


/**
 * @brief Console command "ps" implementation
 *
 * This function is called when the "ps" command is entered.
 * It prints a list of all FreeRTOS tasks with their status, priority, stack usage, etc.
 */

static int ps_cmd(int argc, char **argv)
{
    UBaseType_t uxTaskCount = uxTaskGetNumberOfTasks();
    char pcWriteBuffer[uxTaskCount * 128];
    // table header
    printf("%-24s\tState\tPrior\tStack\tnum", "Task_Name");
    vTaskList(pcWriteBuffer);        //
    printf("\n%s\n", pcWriteBuffer); //
    printf("%-24s\tRuntime\t\tcpu%%", "Task_Name");
    vTaskGetRunTimeStats(pcWriteBuffer);
    printf("\n%s\n", pcWriteBuffer);
    return 0;
}
void register_ps_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "ps",
        .help = "Prints a list of all FreeRTOS tasks with their status, priority, and stack usage.",
        .hint = NULL,
        .func = &ps_cmd,
    };
    // 注册命令
    esp_console_cmd_register(&cmd);
}

static int date_cmd(int argc, char **argv)
{
    struct timeval tv;
    struct tm *tm_info;
    char buffer[64];

    if (argc == 1) {
        // 显示当前时间
        gettimeofday(&tv, NULL);
        tm_info = localtime(&tv.tv_sec);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("Current time: %s\n", buffer);
    } else if (argc == 2 && strcmp(argv[1], "sync") == 0) {
        // 同步SNTP时间
        printf("Syncing time with SNTP server immediately\n");
    	sntp_stop();
    	sntp_init();
    } else if (argc == 3 && strcmp(argv[1], "set") == 0) {
        // 设置系统时间
        struct tm tm_info = {0};
        if (sscanf(argv[2], "%d-%d-%d %d:%d:%d", 
                   &tm_info.tm_year, &tm_info.tm_mon, &tm_info.tm_mday, 
                   &tm_info.tm_hour, &tm_info.tm_min, &tm_info.tm_sec) == 6) {
            // 调整时间格式
            tm_info.tm_year -= 1900;
            tm_info.tm_mon -= 1;
            // 计算时间戳
            time_t time = mktime(&tm_info);
            if (time != -1) {
                struct timeval tv = {time, 0};
                if (settimeofday(&tv, NULL) == 0) {
                    printf("Time set successfully: %s\n", argv[2]);
                } else {
                    printf("Failed to set time\n");
                    return -1;
                }
            } else {
                printf("Invalid time format\n");
                return -1;
            }
        } else {
            printf("Usage: date set YYYY-MM-DD HH:MM:SS\n");
            return -1;
        }
    } else {
        printf("Usage: date [sync|set YYYY-MM-DD HH:MM:SS]\n");
        printf("  date          - Show current time\n");
        printf("  date sync     - Sync time with SNTP server immediately\n");
        printf("  date set ...  - Set system time\n");
        return -1;
    }
    return 0;
}

void register_date_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "date",
        .help = "Show, set, or sync system time",
        .hint = NULL,
        .func = &date_cmd,
    };
    esp_console_cmd_register(&cmd);
}

void register_system_commands(void)
{
    register_free_commands();
    register_ps_cmd();
    register_date_cmd();
}
