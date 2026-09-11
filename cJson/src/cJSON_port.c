#include "FreeRTOS.h"
#include "task.h"
#include "cJSON.h"
void init_cjson_heap_hooks(void)
{
    cJSON_Hooks hooks = {
        .malloc_fn = pvPortMalloc,
        .free_fn = vPortFree,
    };
    cJSON_InitHooks(&hooks);
}
