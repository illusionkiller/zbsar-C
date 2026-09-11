#include "../../logc/src/log.h"
#include "FreeRTOS.h"
#include "semphr.h"

void log_lock(bool lock, void *udata)
{
    SemaphoreHandle_t Mutex = udata;
    if (lock)
        xSemaphoreTake(Mutex, portMAX_DELAY);
    else
        xSemaphoreGive(Mutex);
}

void log_init(void)
{
    SemaphoreHandle_t xMutex = xSemaphoreCreateMutex();
    configASSERT(xMutex != NULL);
    log_set_lock(log_lock, xMutex);
    log_set_level(LOG_DEBUG);
}
