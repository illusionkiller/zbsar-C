#ifndef BNC_CHANNEL_H
#define BNC_CHANNEL_H
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sys.h"
#include "bnc_gen.h"

#define BNC_CHANNEL_NUM 1
#define SYNC_BNC_CHANNEL_MAX_FRAME_SIZE 256
#define BNC_CHANNEL_NOTIFY_TRIGGER_VNA (1UL << 0)
#define BNC_CHANNEL_NOTIFY_TRIGGER_TR (1UL << 1)
#define BNC_CHANNEL_NOTIFY_CLOSE (1UL << 2)
typedef struct
{
    int id;
    int status;
    volatile sys_thread_t recv_thread;
    TaskHandle_t notify_task;
    uint8_t proto_buffer[SYNC_BNC_CHANNEL_MAX_FRAME_SIZE];
    void (*event_callback)(void *channel_handle, u32 event);
    void *user_data;
} bnc_channel_t;

void bnc_channel_init(void);
bnc_channel_t *bnc_channel_get_handle(int channel_id);
int bnc_channel_open(bnc_channel_t *channel);
void bnc_channel_enable(bnc_channel_t *channel);
void bnc_channel_disable(bnc_channel_t *channel);
void bnc_channel_clr_cnt(bnc_channel_t *channel);
u32 bnc_channel_get_out1_cnt(bnc_channel_t *channel);
u32 bnc_channel_get_in1_cnt(bnc_channel_t *channel);
void bnc_channel_close(bnc_channel_t *channel);
bool bnc_channel_is_opened(bnc_channel_t *channel);
int bnc_channel_config(bnc_channel_t *channel, BNC_GEN_Config *config);
void bnc_channel_get_config(bnc_channel_t *channel, BNC_GEN_Config *config);
void bnc_channel_register_event_callback(bnc_channel_t *channel, void (*event_callback)(void *channel_handle, u32 event), void *user_data);
void bnc_channel_start(bnc_channel_t *channel);
void bnc_channel_config_num(bnc_channel_t *channel, u32 num);
void bnc_channel_in1_enable(bnc_channel_t *channel);
void bnc_channel_in1_disable(bnc_channel_t *channel);

#endif
