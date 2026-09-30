#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include <stdlib.h>
#include "time.h"
#include <sys/time.h>
#include "sntp.h"
#include "lwip/inet.h"
#include "string.h"
#include "sleep.h"
#include "env.h"

#define DEFAULT_NTP_SERVER_IP "192.168.1.215"
#define USE_FREERTOS_TIMER

#ifdef USE_FREERTOS_TIMER 
static TimerHandle_t xTimer = NULL;
static uint64_t local_time_us = 0;
static uint32_t local_tick_time = 1;
#else
#include "xtime_l.h"
static    XTime tCur = 0;
#endif
int gettimeofday(struct timeval *tv, void *tzvp)
{
    (void)tzvp; // unused

    // TickType_t ticks = xTaskGetTickCount();
    // tv->tv_sec = ticks / configTICK_RATE_HZ;
//    tv->tv_usec = (ticks % configTICK_RATE_HZ) * (1000000 / configTICK_RATE_HZ);
#ifdef USE_FREERTOS_TIMER
    tv->tv_sec = local_time_us / 1000000;
    tv->tv_usec = local_time_us % 1000000;
#else
    XTime tCur = 0;
    XTime_GetTime(&tCur);
    tv->tv_sec = tCur / COUNTS_PER_SECOND;
    tv->tv_usec = (tCur % COUNTS_PER_SECOND) * 1000000 / COUNTS_PER_SECOND;
#endif
    return 0;
}

int _gettimeofday(struct timeval *tv, void *tzvp)
{
    (void)tzvp; // unused

    // TickType_t ticks = xTaskGetTickCount();
    // tv->tv_sec = ticks / configTICK_RATE_HZ;
//    tv->tv_usec = (ticks % configTICK_RATE_HZ) * (1000000 / configTICK_RATE_HZ);
#ifdef USE_FREERTOS_TIMER
    tv->tv_sec = local_time_us / 1000000;
    tv->tv_usec = local_time_us % 1000000;
#else
    XTime tCur = 0;
    XTime_GetTime(&tCur);
    tv->tv_sec = tCur / COUNTS_PER_SECOND;
    tv->tv_usec = (tCur % COUNTS_PER_SECOND) * 1000000 / COUNTS_PER_SECOND;
#endif
    return 0;
}

int settimeofday (const struct timeval *tv, const struct timezone *tz)
{
#ifdef USE_FREERTOS_TIMER
    local_time_us = tv->tv_sec * 1000000 + tv->tv_usec;
#else
    tCur = tv->tv_sec * COUNTS_PER_SECOND + tv->tv_usec * COUNTS_PER_SECOND / 1000000;
    XTime_SetTime(tCur);
#endif
    return 0;
}

void sntp_set_system_time_us(uint32_t sec, uint32_t us)
{
    struct timeval tv = {sec, us};
    settimeofday(&tv, NULL);
}

#ifdef USE_FREERTOS_TIMER
void TimerCallback(TimerHandle_t xTimer) {
    local_time_us += local_tick_time  * 1000; // 1 tick to us
}
#endif

void sntp_client_init(void)
{
	sntp_setoperatingmode(SNTP_OPMODE_POLL);
	ip_addr_t ip;
    char *server_ip = getenv("ntpserver");
    if (server_ip == NULL)
    {
        server_ip = DEFAULT_NTP_SERVER_IP;
    }
	inet_aton(server_ip, &ip);
	sntp_setserver(0, &ip); // Or local NTP server
	sntp_stop();
	sntp_init();
	// 将时区设置为中国标准时间
	setenv("TZ", "CST-8", 1);
	tzset();
#ifdef USE_FREERTOS_TIMER
    //启动一个ms定时器
    if(xTimer == NULL)
    {
        local_tick_time = portTICK_PERIOD_MS;
        xTimer = xTimerCreate("Timer",1,pdTRUE, (void *)0, TimerCallback ); 
        xTimerStart(xTimer, 0);
    }
#endif
}

void set_system_time_to_compile_time(void)
{
    struct tm tm_info = {0};
    char month[4];
    int day, year, hour, min, sec;

    // 解析__DATE__格式: "MMM DD YYYY"
    sscanf(__DATE__, "%3s %d %d", month, &day, &year);
    // 解析__TIME__格式: "HH:MM:SS"
    sscanf(__TIME__, "%d:%d:%d", &hour, &min, &sec);

    // 设置tm结构体
    memset(&tm_info, 0, sizeof(tm_info));
    tm_info.tm_mday = day;
    tm_info.tm_year = year - 1900;
    tm_info.tm_hour = hour;
    tm_info.tm_min = min;
    tm_info.tm_sec = sec;
    tm_info.tm_mon = 0;
    const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++)
    {
        if (strcmp(month, month_names[i]) == 0)
        {
            tm_info.tm_mon = i;
            break;
        }
    }
    // 计算时间戳并设置系统时间
    time_t compile_time = mktime(&tm_info);
    struct timeval tv = {compile_time, 0};
    settimeofday(&tv, NULL);
}
