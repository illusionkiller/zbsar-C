// Copyright (c) Sandeep Mistry. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef SYNC_RS422RX_CHANNEL_H
#define SYNC_RS422RX_CHANNEL_H
#include "bnc_channel.h"
#include "spi_pl_slave.h"

#define SYNC_RS422RX_CHANNEL_NUM 6
#define SYNC_RS422RX_CHANNEL_MAX_FRAME_SIZE SPI_PL_SLAVE_FIFO_DEPTH

typedef struct
{
    int id;
    int spi_bus_id;
    int bnc_channel_id;
    spi_pl_slave_bus_t *spi_bus;
    int status;
    volatile sys_thread_t recv_thread;
//    MessageBufferHandle_t xMessageBuffer;
    // TaskHandle_t notify_task;
    uint8_t *proto_buffer;
    void *proto_msg;
    void (*event_callback)(void *channel_handle, u32 event);
    void *user_data;
    bnc_channel_t *bnc_channel;
    int  request_data_num;
} sync_rs422rx_channel_t;

void sync_rs422rx_channel_init(void);
sync_rs422rx_channel_t *sync_rs422rx_channel_get_handle(int channel_id);
int sync_rs422rx_channel_open(sync_rs422rx_channel_t *channel);
void sync_rs422rx_channel_close(sync_rs422rx_channel_t *channel);
int package_sync_rs422rx_channel_resp_msg(sync_rs422rx_channel_t *channel, uint32_t message_id, int error_code);
bool sync_rs422rx_channel_is_opened(sync_rs422rx_channel_t *channel);
void sync_rs422rx_channel_register_event_callback(sync_rs422rx_channel_t *channel, void (*event_callback)(void *channel_handle, u32 event), void *user_data);
int sync_rs422rx_channel_trigger_query_data(sync_rs422rx_channel_t *channel,int num);
#endif
