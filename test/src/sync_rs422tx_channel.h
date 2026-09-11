// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SYNC_RS422TX_CHANNEL_H
#define SYNC_RS422TX_CHANNEL_H

#include "bnc_channel.h"
#include "spi_pl.h"
#include "pwm_tr.h"

#define SYNC_RS422TX_CHANNEL_NUM 6
#define SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE SPI_PL_FIFO_DEPTH
#define SYNC_RS422TX_CHANNEL_MAX_RECV_SIZE 1024
#define SYNC_RS422TX_NOTIFY_SPI_DONE (1UL << 0)
#define SYNC_RS422TX_NOTIFY_PWM_DONE (1UL << 1)
#define SYNC_RS422TX_NOTIFY_CLOSE (1UL << 2)
typedef struct
{
    int id;
    int spi_bus_id;
    int pwm_tr_dev_id;
    int bnc_channel_id;
    spi_pl_bus_t *spi_bus;
    int status;
    volatile sys_thread_t recv_thread;
    // MessageBufferHandle_t xMessageBuffer;
    // TaskHandle_t notify_task;
    uint8_t *proto_buffer;
    void *proto_msg;
    void (*event_callback)(void *channel_handle, u32 event);
    void *user_data;
    bnc_channel_t *bnc_channel;
} sync_rs422tx_channel_t;

void sync_rs422tx_channel_init(void);
sync_rs422tx_channel_t *sync_rs422tx_channel_get_handle(int channel_id);
int sync_rs422tx_channel_open(sync_rs422tx_channel_t *channel);
void sync_rs422tx_channel_close(sync_rs422tx_channel_t *channel);
int sync_rs422tx_channel_transfer(sync_rs422tx_channel_t *channel, uint8_t *senddata[4], size_t len, size_t bits_len, uint32_t timeout_ms,uint32_t frame_num);
bool sync_rs422tx_channel_is_opened(sync_rs422tx_channel_t *channel);
int sync_rs422tx_channel_write(sync_rs422tx_channel_t *channel, uint8_t *senddata[4], size_t len, size_t bits_len,uint32_t frame_num);
void sync_rs422tx_channel_start_transfer(sync_rs422tx_channel_t *channel);
void sync_rs422tx_channel_register_event_callback(sync_rs422tx_channel_t *channel, void (*event_callback)(void *channel_handle, u32 event), void *user_data);
#endif
