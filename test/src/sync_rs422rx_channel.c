#include "sync_rs422rx_channel.h"
#include "queue.h"
#include "message_buffer.h"
#include "lwip/sys.h"
#include "nanopb/pb_decode.h"
#include "nanopb/dtu.pb.h"
#include "nanopb/pb_encode.h"
#include "vfs.h"
#include "env.h"
#include "dtu.h"
#include "cJSON.h"
#include "log.h"



#define PROTO_MSG_HEAD_SIZE 256
#define PROTO_SUB4MSG_SIZE (SubMsg4_size + PROTO_MSG_HEAD_SIZE)
#define SYNC_RS422RX_NOTIFY_RECV_DONE (1UL << 0)
#define SYNC_RS422RX_NOTIFY_CLOSE (1UL << 1)
#define SYNC_RS422RX_NOTIFY_TRIGGER_RESPONSE_DATA (1UL << 2)
static void sync_rs422rx_event_callback(void *channel_handle, u32 event)
{
    sync_rs422rx_channel_t *channel = (sync_rs422rx_channel_t *)channel_handle;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    configASSERT(channel != NULL);
    if (event != SPI_PL_SLAVE_BUS_EVENT_RECEIVE_DONE || channel->recv_thread == NULL)
    {
        return;
    }
    BNC_GEN_Config bnc_config  = {0};
    bnc_channel_get_config(channel->bnc_channel, &bnc_config);
    if (bnc_config.mode == BNC_GEN_MODE_SLAVE_PARALLEL)
    {
		if(channel->request_data_num == 0)
		{
			spi_pl_slave_bus_reset_rxfifo(channel->spi_bus);
		}
	}
    xTaskNotifyFromISR(channel->recv_thread, SYNC_RS422RX_NOTIFY_RECV_DONE, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Package RS422 RX channel data into a protobuf message.
static int package_sync_rs422rx_channel_req_msg(sync_rs422rx_channel_t *channel, uint8_t *data[5], size_t data_len, int mode)
{
    OneOfMessage *msg;
    pb_ostream_t stream;
    struct timeval tv;

    if (channel == NULL || data == NULL || data_len == 0)
    {
        return 0;
    }

    msg = channel->proto_msg;
    configASSERT(msg != NULL);
    memset(msg, 0, sizeof(*msg));

    if (data_len > SYNC_RS422RX_CHANNEL_MAX_FRAME_SIZE)
    {
        return 0;
    }

    msg->ver = 1;
    msg->type = ChannelType_SYNC_RS422_RX;
    msg->which_values = OneOfMessage_submsg4_tag;
    long long milliseconds = 0;
    if (mode != BNC_GEN_MODE_SLAVE_PARALLEL)
    {
        gettimeofday(&tv, NULL);
        milliseconds = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    }
    msg->id = milliseconds;
    msg->values.submsg4.timestamp = milliseconds;
    msg->values.submsg4.channel = channel->id;
    switch(channel->id)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
        if(mode == BNC_GEN_MODE_SLAVE_PARALLEL)
        {
            msg->values.submsg4.data0.size = 0;
            msg->values.submsg4.data1.size = 0;
            msg->values.submsg4.data2.size = 0;
            msg->values.submsg4.data3.size = 0;
        }
        else
        {
            msg->values.submsg4.data0.size = data_len;
            msg->values.submsg4.data1.size = data_len;
            msg->values.submsg4.data2.size = data_len;
            msg->values.submsg4.data3.size = data_len;
        }
        msg->values.submsg4.data4.size = data_len;
        memcpy(msg->values.submsg4.data0.bytes, data[0], msg->values.submsg4.data0.size);
        memcpy(msg->values.submsg4.data1.bytes, data[1], msg->values.submsg4.data1.size);
        memcpy(msg->values.submsg4.data2.bytes, data[2], msg->values.submsg4.data2.size);
        memcpy(msg->values.submsg4.data3.bytes, data[3], msg->values.submsg4.data3.size);
        memcpy(msg->values.submsg4.data4.bytes, data[4], msg->values.submsg4.data4.size);
        break;
    default:break;
    }

    stream = pb_ostream_from_buffer(channel->proto_buffer, PROTO_SUB4MSG_SIZE);
    if (!pb_encode(&stream, OneOfMessage_fields, msg))
    {
        log_error("Encoding sync RS422RX channel %d req frame failed: %s", channel->id, PB_GET_ERROR(&stream));
        return 0;
    }
    return stream.bytes_written;
}

// RX loopback receive thread.
static void sync_rs422rx_recv_thread(void *arg)
{
    sync_rs422rx_channel_t *channel = (sync_rs422rx_channel_t *)arg;
    configASSERT(channel != NULL);
    channel->bnc_channel = bnc_channel_get_handle(channel->bnc_channel_id);
    configASSERT(channel->bnc_channel != NULL);
    BNC_GEN_Config bnc_config = DEFAULT_BNC_GEN_CONFIG();
    bnc_channel_get_config(channel->bnc_channel, &bnc_config);

    uint8_t (*recvdata)[SYNC_RS422RX_CHANNEL_MAX_FRAME_SIZE] = NULL;
    recvdata = pvPortMalloc(sizeof(*recvdata) * 5);
    configASSERT(recvdata != NULL);
    memset(recvdata, 0, sizeof(*recvdata) * 5);
    uint8_t *last_data[5] = {recvdata[0], recvdata[1], recvdata[2], recvdata[3], recvdata[4]};

    channel->proto_buffer = pvPortMalloc(PROTO_SUB4MSG_SIZE);
    configASSERT(channel->proto_buffer != NULL);
    memset(channel->proto_buffer, 0, PROTO_SUB4MSG_SIZE);
    channel->proto_msg = pvPortMalloc(sizeof(OneOfMessage));
    configASSERT(channel->proto_msg != NULL);
    memset(channel->proto_msg, 0, sizeof(*channel->proto_msg));

    spi_pl_slave_bus_bind_irq_callback(channel->spi_bus, sync_rs422rx_event_callback, channel);
    spi_pl_slave_bus_reset_fifo(channel->spi_bus);
    spi_pl_slave_bus_enable(channel->spi_bus, true);
    log_info("Sync RS422RX channel %d open success", channel->id);
    int last_len = 0;
    while (1)
    {
        uint32_t notify_value = 0;

        if (xTaskNotifyWait(0, 0xffffffffUL, &notify_value, portMAX_DELAY) != pdPASS)
        {
            continue;
        }
        if ((notify_value & SYNC_RS422RX_NOTIFY_CLOSE) != 0)
        {
            break;
        }
        if ((notify_value & SYNC_RS422RX_NOTIFY_RECV_DONE) != 0)
        {
            if (channel->event_callback != NULL)
            {
                channel->event_callback(channel->user_data, SYNC_RS422RX_NOTIFY_RECV_DONE);
            }
            bnc_channel_get_config(channel->bnc_channel, &bnc_config);
            if (bnc_config.mode == BNC_GEN_MODE_SLAVE_PARALLEL)
            {
                if(channel->request_data_num)
                {
                    last_len = spi_pl_slave_bus_recv_data4(channel->spi_bus, last_data[4], channel->request_data_num);
                    if (last_len == channel->request_data_num)
                    {
                        int pkg_len = package_sync_rs422rx_channel_req_msg(channel, last_data, last_len, bnc_config.mode);
                        if (pkg_len > 0)
                        {
                            dtu_send_data(channel->proto_buffer, pkg_len);
                        }
                    }
                	channel->request_data_num = 0;
                }
//                else
//                {
//                    spi_pl_slave_bus_reset_rxfifo(channel->spi_bus);
//                }
            }
            else
            {
                last_len = spi_pl_slave_bus_recv(channel->spi_bus, last_data, SYNC_RS422RX_CHANNEL_MAX_FRAME_SIZE);
                if (last_len <= 0)
                {
                    continue;
                }
                if (spi_pl_slave_bus_write(channel->spi_bus, last_data, last_len) == last_len)
                {
                    spi_pl_slave_bus_start_transfer(channel->spi_bus);
                }
                int pkg_len = package_sync_rs422rx_channel_req_msg(channel, last_data, last_len, bnc_config.mode);
                if (pkg_len > 0)
                {
                    dtu_send_data(channel->proto_buffer, pkg_len);
                }
            }
        }
        // if((notify_value & SYNC_RS422RX_NOTIFY_TRIGGER_RESPONSE_DATA) != 0)
        // {

        // }
    }

    spi_pl_slave_bus_enable(channel->spi_bus, false);
    spi_pl_slave_bus_bind_irq_callback(channel->spi_bus, NULL, NULL);
    spi_pl_slave_bus_reset_fifo(channel->spi_bus);
    vPortFree(recvdata);
    recvdata = NULL;
    vPortFree(channel->proto_msg);
    channel->proto_msg = NULL;
    vPortFree(channel->proto_buffer);
    channel->proto_buffer = NULL;
    channel->recv_thread = NULL;
    vTaskDelete(NULL);
}

int sync_rs422rx_channel_open(sync_rs422rx_channel_t *channel)
{
    if(sync_rs422rx_channel_is_opened(channel))
    {
        log_warn("Sync RS422RX channel %d already opened", channel->id);
        return 0;
    }

    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "sync_rs422rx_recv_channel_%d", channel->id);
    channel->recv_thread = sys_thread_new(thread_name, sync_rs422rx_recv_thread, channel,
                                          configMINIMAL_STACK_SIZE * 2, tskIDLE_PRIORITY + 8);
    if (channel->recv_thread == NULL)
    {
        log_error("Sync RS422RX channel %d create recv thread failed", channel->id);
        return -1;
    }
    return 0;
}

void sync_rs422rx_channel_close(sync_rs422rx_channel_t *channel)
{
    if(!sync_rs422rx_channel_is_opened(channel))
    {
        log_warn("Sync RS422RX channel %d already closed", channel->id);
        return;
    }

    xTaskNotify(channel->recv_thread, SYNC_RS422RX_NOTIFY_CLOSE, eSetBits);
    while (channel->recv_thread != NULL)
    {
//        taskYIELD(); // 等待端口彻底关闭，资源释放完成后再打开端口
    	vTaskDelay(pdMS_TO_TICKS(1));
    }
    log_info("Sync RS422RX channel %d closed", channel->id);
}

static sync_rs422rx_channel_t sync_rs422rx_channel[SYNC_RS422RX_CHANNEL_NUM] = {
    {
        .id = 0,
        .spi_bus_id = 0,
        .bnc_channel_id = 0,
    },
    {
        .id = 1,
        .spi_bus_id = 1,
        .bnc_channel_id = 0,
    },
    {
        .id = 2,
        .spi_bus_id = 2,
        .bnc_channel_id = 0,
    },
    {
        .id = 3,
        .spi_bus_id = 3,
        .bnc_channel_id = 0,

    },
    {
        .id = 4,
        .spi_bus_id = 4,
        .bnc_channel_id = 0,

    },
};

sync_rs422rx_channel_t *sync_rs422rx_channel_get_handle(int channel_id)
{
    int count = sizeof(sync_rs422rx_channel) / sizeof(sync_rs422rx_channel[0]);
    for (int i = 0; i < count; i++)
    {
        if (sync_rs422rx_channel[i].id == channel_id)
        {
            return &sync_rs422rx_channel[i];
        }
    }
    return NULL;
}

void sync_rs422rx_channel_init(void)
{
    spi_pl_slave_bus_init();
    for (int i = 0; i < sizeof(sync_rs422rx_channel) / sizeof(sync_rs422rx_channel[0]); i++)
    {
        sync_rs422rx_channel[i].spi_bus = spi_pl_slave_bus_get_handle(sync_rs422rx_channel[i].spi_bus_id);
        if (sync_rs422rx_channel[i].spi_bus == NULL)
        {
            log_error("sync RS422RX channel%d bind spi_bus%d failed", i, sync_rs422rx_channel[i].spi_bus_id);
            continue;
        }
    }
    log_info("sync RS422RX channel init done.");
}

bool sync_rs422rx_channel_is_opened(sync_rs422rx_channel_t *channel)
{
    return (channel != NULL) && (channel->recv_thread != NULL);
}

// Register the channel event callback.
void sync_rs422rx_channel_register_event_callback(sync_rs422rx_channel_t *channel, void (*callback)(void *handle, u32 event),void *user_data)
{
    channel->event_callback = callback;
    channel->user_data = user_data;
}

int sync_rs422rx_channel_trigger_query_data(sync_rs422rx_channel_t *channel,int num)
{
    if(!sync_rs422rx_channel_is_opened(channel))
    {
        log_warn("Sync RS422RX channel %d not opened", channel->id);
        return -1;
    }
    channel->request_data_num = num;
//     if(channel->request_data_num)
//     xTaskNotify(channel->recv_thread,
//    		 SYNC_RS422RX_NOTIFY_RECV_DONE,
//                 eSetBits);
    return 0;
}
