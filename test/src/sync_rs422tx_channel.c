#include "sync_rs422tx_channel.h"
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
#define PROTO_SUB1MSG_SIZE (SubMsg1_size + PROTO_MSG_HEAD_SIZE)
#define PROTO_SUB2MSG_SIZE (SubMsg2_size + PROTO_MSG_HEAD_SIZE)
static int parse_sync_rs422tx_channel_config_from_file(int channel_id, const char *file_name, spi_pl_config_t *config, pulse_Config *tr_config);
// Response/event protobuf message builder.
static int package_sync_rs422tx_channel_resp_msg(sync_rs422tx_channel_t *channel, uint32_t message_id, int error_code, int mode);

static void sync_rs422tx_event_callback(void *channel_handle, u32 event)
{
    sync_rs422tx_channel_t *channel = (sync_rs422tx_channel_t *)channel_handle;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    configASSERT(channel != NULL);
    if (event != SPI_PL_BUS_EVENT_TRANSFER_DONE || channel->recv_thread == NULL)
    {
        return;
    }

    xTaskNotifyFromISR(channel->recv_thread, SYNC_RS422TX_NOTIFY_SPI_DONE, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void pwm_tr_event_callback(void *channel_handle, u32 event)
{
    sync_rs422tx_channel_t *channel = (sync_rs422tx_channel_t *)channel_handle;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    configASSERT(channel != NULL);
    if (event != PWM_TR_EVENT_TRANSFER_DONE || channel->recv_thread == NULL)
    {
        return;
    }

    xTaskNotifyFromISR(channel->recv_thread, SYNC_RS422TX_NOTIFY_PWM_DONE, eSetBits, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static int sync_rs422tx_dtu_handler(OneOfMessage *msg, void *user_data)
{
    sync_rs422tx_channel_t *channel = (sync_rs422tx_channel_t *)user_data;

    if (!sync_rs422tx_channel_is_opened(channel))
    {
        log_error("Sync RS422TX channel %d is not opened", channel->id);
        return -1;
    }
    if (msg == NULL || channel == NULL)
    {
        return -1;
    }

    if (msg->which_values != OneOfMessage_submsg1_tag)
    {
        return -1;
    }

    if (msg->values.submsg1.channel != channel->id)
    {
        return -1;
    }

    uint8_t *data[4] = {NULL, NULL, NULL, NULL};
    data[0] = msg->values.submsg1.data0.size > 0 ? msg->values.submsg1.data0.bytes : NULL;
    data[1] = msg->values.submsg1.data1.size > 0 ? msg->values.submsg1.data1.bytes : NULL;
    data[2] = msg->values.submsg1.data2.size > 0 ? msg->values.submsg1.data2.bytes : NULL;
    data[3] = msg->values.submsg1.data3.size > 0 ? msg->values.submsg1.data3.bytes : NULL;

    size_t data_len = msg->values.submsg1.data0.size > msg->values.submsg1.data1.size ? msg->values.submsg1.data0.size : msg->values.submsg1.data1.size;
    data_len = data_len > msg->values.submsg1.data2.size ? data_len : msg->values.submsg1.data2.size;
    data_len = data_len > msg->values.submsg1.data3.size ? data_len : msg->values.submsg1.data3.size;
    if ((data_len == 0) || (msg->values.submsg1.frame_num == 0) || (msg->values.submsg1.frame_num > data_len) || (data_len > SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE))
    {
        log_error("frame_num %d is invalid", msg->values.submsg1.frame_num);
        return -1;
    }
    uint32_t max_trans_bits = (data_len * 8 / msg->values.submsg1.frame_num);
    uint32_t min_trans_bits = max_trans_bits - 7;
    if ((msg->values.submsg1.trans_bits < min_trans_bits) || (msg->values.submsg1.trans_bits > max_trans_bits))
    {
        log_error("trans_bits %d is invalid,should be in (%d-%d)", msg->values.submsg1.frame_num,min_trans_bits,max_trans_bits);
        return -1;
    }
    int tx_len = sync_rs422tx_channel_transfer(channel, data, data_len, msg->values.submsg1.trans_bits, 0, msg->values.submsg1.frame_num);
    if (tx_len != (int)data_len)
    {
        log_error("Failed to send sync rs422 channel %d data, err:%d", channel->id, tx_len);
        /* 响应消息使用独立缓冲区，可由 DTU 线程安全发送。 */
        BNC_GEN_Config bnc_config = DEFAULT_BNC_GEN_CONFIG();
        bnc_channel_get_config(channel->bnc_channel, &bnc_config);
        (void)package_sync_rs422tx_channel_resp_msg(channel, 0, tx_len, bnc_config.mode);
        return -1;
    }

    return 0;
}

// Package RS422 TX receive data into a protobuf message.
static int package_sync_rs422tx_channel_req_msg(sync_rs422tx_channel_t *channel, uint8_t *data[4], size_t data_len, int mode)
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

    if (data_len > SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE)
    {
        return 0;
    }

    msg->ver = 1;
    msg->type = ChannelType_SYNC_RS422_TX;
    msg->which_values = OneOfMessage_submsg1_tag;
    long long milliseconds = 0;
    if (mode != BNC_GEN_MODE_SLAVE_PARALLEL)
    {
        gettimeofday(&tv, NULL);
        milliseconds = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    }
    msg->id = milliseconds;
    msg->values.submsg1.timestamp = milliseconds;
    msg->values.submsg1.channel = channel->id;
    msg->values.submsg1.data0.size = data_len;
    msg->values.submsg1.trans_bits = data_len * 8;
    memcpy(msg->values.submsg1.data0.bytes, data[0], data_len);
    msg->values.submsg1.data1.size = data_len;
    memcpy(msg->values.submsg1.data1.bytes, data[1], data_len);
    msg->values.submsg1.data2.size = data_len;
    memcpy(msg->values.submsg1.data2.bytes, data[2], data_len);
    msg->values.submsg1.data3.size = data_len;
    memcpy(msg->values.submsg1.data3.bytes, data[3], data_len);
    stream = pb_ostream_from_buffer(channel->proto_buffer, PROTO_SUB1MSG_SIZE);
    if (!pb_encode(&stream, OneOfMessage_fields, msg))
    {
        log_error("Encoding sync RS422TX channel %d req frame failed: %s", channel->id, PB_GET_ERROR(&stream));
        return 0;
    }

    return stream.bytes_written;
}

// Package RS422 TX response/event data into a protobuf message.
// The response message uses a private protobuf buffer because the DTU handler may run concurrently.
// Do not reuse the channel request message or buffer while encoding the response.
// Response/event protobuf message builder.
static int package_sync_rs422tx_channel_resp_msg(sync_rs422tx_channel_t *channel, uint32_t message_id, int error_code, int mode)
{

    pb_ostream_t stream;
    struct timeval tv;
    int ret = 0;
    configASSERT(channel != NULL);
    uint8_t *encode_buf = pvPortMalloc(PROTO_SUB2MSG_SIZE);
    configASSERT(encode_buf != NULL);
    memset(encode_buf, 0, PROTO_SUB2MSG_SIZE);
    OneOfMessage *msg = pvPortMalloc(sizeof(*msg));
    configASSERT(msg != NULL);
    memset(msg, 0, sizeof(*msg));

    (void)message_id;
    msg->ver = 1;
    msg->type = ChannelType_SYNC_RS422_TX;
    msg->which_values = OneOfMessage_submsg2_tag;
    long long milliseconds = 0;
    if (mode != BNC_GEN_MODE_SLAVE_PARALLEL)
    {
        gettimeofday(&tv, NULL);
        milliseconds = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    }
    msg->id = milliseconds;
    msg->values.submsg2.timestamp = milliseconds;
    msg->values.submsg2.channel = channel->id;
    msg->values.submsg2.event_code = error_code;
    stream = pb_ostream_from_buffer(encode_buf, PROTO_SUB2MSG_SIZE);
    if (!pb_encode(&stream, OneOfMessage_fields, msg))
    {
        log_error("Encoding sync RS422TX channel %d resp frame failed: %s", channel->id, PB_GET_ERROR(&stream));
    }
    else
    {
        dtu_send_data(encode_buf, stream.bytes_written);
        ret = stream.bytes_written;
    }
    vPortFree(encode_buf);
    vPortFree(msg);
    return ret;
}
// TX receive/event thread.
void sync_rs422tx_recv_thread(void *arg)
{
    sync_rs422tx_channel_t *channel = (sync_rs422tx_channel_t *)arg;
    configASSERT(channel != NULL);
    spi_pl_config_t config = DEFAULT_SPI_PL_BUS_CONFIG();
    pulse_Config tr_config = DEFAULT_PWM_TR_CONFIG();
    channel->bnc_channel = bnc_channel_get_handle(channel->bnc_channel_id);
    configASSERT(channel->bnc_channel != NULL);
    BNC_GEN_Config bnc_config = DEFAULT_BNC_GEN_CONFIG();
    bnc_channel_get_config(channel->bnc_channel, &bnc_config);

    uint8_t (*recvdata)[SYNC_RS422TX_CHANNEL_MAX_RECV_SIZE] = NULL;
    recvdata = pvPortMalloc(sizeof(*recvdata) * 4);
    configASSERT(recvdata != NULL);
    memset(recvdata, 0, sizeof(*recvdata) * 4);

    channel->proto_buffer = pvPortMalloc(PROTO_SUB1MSG_SIZE);
    configASSERT(channel->proto_buffer != NULL);
    memset(channel->proto_buffer, 0, PROTO_SUB1MSG_SIZE);
    channel->proto_msg = pvPortMalloc(sizeof(OneOfMessage));
    configASSERT(channel->proto_msg != NULL);
    memset(channel->proto_msg, 0, sizeof(*channel->proto_msg));

    parse_sync_rs422tx_channel_config_from_file(channel->id, getenv(TEST_PARAM_FILE_NAME), &config, &tr_config);
    pwm_tr_dev_config(channel->pwm_tr_dev_id, &tr_config);
    spi_pl_bus_config(channel->spi_bus, &config);
    spi_pl_bus_select(channel->spi_bus, 0);
    spi_pl_bus_bind_irq_callback(channel->spi_bus, sync_rs422tx_event_callback, channel);
    spi_pl_bus_enable(channel->spi_bus, true);
    spi_pl_bus_reset_fifo(channel->spi_bus);
    pwm_tr_dev_open(channel->pwm_tr_dev_id, 0);
    pwm_tr_dev_bind_irq_callback(channel->pwm_tr_dev_id, pwm_tr_event_callback, channel);
    dtu_register_message_handler(ChannelType_SYNC_RS422_TX, OneOfMessage_submsg1_tag,
                                 sync_rs422tx_dtu_handler, channel);
    log_info("Sync RS422TX channel %d open success", channel->id);

    while (1)
    {
        uint32_t notify_value = 0;

        if (xTaskNotifyWait(0, 0xffffffffUL, &notify_value, portMAX_DELAY) != pdPASS)
        {
            continue;
        }
        if ((notify_value & SYNC_RS422TX_NOTIFY_CLOSE) != 0)
        {
            break;
        }

        if ((notify_value & SYNC_RS422TX_NOTIFY_SPI_DONE) != 0)
        {
            if (channel->event_callback != NULL)
            {
                channel->event_callback(channel->user_data, SYNC_RS422TX_NOTIFY_SPI_DONE);
            }

            if (channel->spi_bus->config.self_loop)
            {
                uint8_t *data[4] = {recvdata[0], recvdata[1], recvdata[2], recvdata[3]};
                int len = spi_pl_bus_recv(channel->spi_bus, data, SYNC_RS422TX_CHANNEL_MAX_RECV_SIZE);
                if (len > 0)
                {
                    int pkg_len = package_sync_rs422tx_channel_req_msg(channel, data, len,bnc_config.mode);
                    if (pkg_len > 0)
                    {
                        dtu_send_data(channel->proto_buffer, pkg_len);
                    }
                }
            }else
            {
                package_sync_rs422tx_channel_resp_msg(channel, 0, SYNC_RS422TX_NOTIFY_SPI_DONE, bnc_config.mode);
            }
        }

        if ((notify_value & SYNC_RS422TX_NOTIFY_PWM_DONE) != 0)
        {
            if (channel->event_callback != NULL)
            {
                channel->event_callback(channel->user_data, SYNC_RS422TX_NOTIFY_PWM_DONE);
            }
            package_sync_rs422tx_channel_resp_msg(channel, 0, SYNC_RS422TX_NOTIFY_PWM_DONE, bnc_config.mode);
        }
    }
    channel->bnc_channel = NULL;
    dtu_unregister_message_handler(ChannelType_SYNC_RS422_TX, OneOfMessage_submsg1_tag,
                                   sync_rs422tx_dtu_handler, channel);
    pwm_tr_dev_close(channel->pwm_tr_dev_id);
    spi_pl_bus_enable(channel->spi_bus, false);
    spi_pl_bus_bind_irq_callback(channel->spi_bus, NULL, NULL);
    pwm_tr_dev_bind_irq_callback(channel->pwm_tr_dev_id, NULL, NULL);
    spi_pl_bus_reset_fifo(channel->spi_bus);
    vPortFree(recvdata);
    recvdata = NULL;
    vPortFree(channel->proto_msg);
    channel->proto_msg = NULL;
    vPortFree(channel->proto_buffer);
    channel->proto_buffer = NULL;
    channel->recv_thread = NULL;
    vTaskDelete(NULL);
}

int sync_rs422tx_channel_open(sync_rs422tx_channel_t *channel)
{
    if (sync_rs422tx_channel_is_opened(channel))
    {
        // log_warn("Sync RS422TX channel %d already opened", channel->id);
        return 0;
    }

    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "sync_rs422tx_recv_channel_%d", channel->id);
    channel->recv_thread = sys_thread_new(thread_name, sync_rs422tx_recv_thread, channel,
                                          configMINIMAL_STACK_SIZE * 2, tskIDLE_PRIORITY + 8);
    if (channel->recv_thread == NULL)
    {
        log_error("Sync RS422TX channel %d create recv thread failed", channel->id);
        return -1;
    }
    return 0;
}

void sync_rs422tx_channel_close(sync_rs422tx_channel_t *channel)
{
    if (!sync_rs422tx_channel_is_opened(channel))
    {
        // log_warn("Sync RS422TX channel %d already closed", channel->id);
        return;
    }

    xTaskNotify(channel->recv_thread, SYNC_RS422TX_NOTIFY_CLOSE, eSetBits);
    while (channel->recv_thread != NULL)
    {
//        taskYIELD(); // 等待端口彻底关闭，资源释放完成后再打开端口
    	vTaskDelay(pdMS_TO_TICKS(1));
    }
    log_info("Sync RS422TX channel %d closed", channel->id);
}

static sync_rs422tx_channel_t sync_rs422tx_channel[SYNC_RS422TX_CHANNEL_NUM] = {
    {
        .id = 0,
        .spi_bus_id = 0,
        .pwm_tr_dev_id = 0,
        .bnc_channel_id = 0,
    },
    {
        .id = 1,
        .spi_bus_id = 1,
        .pwm_tr_dev_id = 1,
        .bnc_channel_id = 0,
    },
    {
        .id = 2,
        .spi_bus_id = 2,
        .pwm_tr_dev_id = 2,
        .bnc_channel_id = 0,
    },
    {
        .id = 3,
        .spi_bus_id = 3,
        .pwm_tr_dev_id = 3,
        .bnc_channel_id = 0,
    },
    {
        .id = 4,
        .spi_bus_id = 4,
        .pwm_tr_dev_id = 4,
        .bnc_channel_id = 0,
    },
};

sync_rs422tx_channel_t *sync_rs422tx_channel_get_handle(int channel_id)
{
    int count = sizeof(sync_rs422tx_channel) / sizeof(sync_rs422tx_channel[0]);
    for (int i = 0; i < count; i++)
    {
        if (sync_rs422tx_channel[i].id == channel_id)
        {
            return &sync_rs422tx_channel[i];
        }
    }
    return NULL;
}

void sync_rs422tx_channel_init(void)
{
    pwm_tr_dev_init();
    spi_pl_bus_init();
    for (int i = 0; i < sizeof(sync_rs422tx_channel) / sizeof(sync_rs422tx_channel[0]); i++)
    {
        sync_rs422tx_channel[i].spi_bus = spi_pl_bus_get_handle(sync_rs422tx_channel[i].spi_bus_id);
        if (sync_rs422tx_channel[i].spi_bus == NULL)
        {
            log_error("sync RS422TX channel%d bind spi_bus%d failed", i, sync_rs422tx_channel[i].spi_bus_id);
            continue;
        }
    }
    log_info("sync RS422TX channel init done.");
}

int sync_rs422tx_channel_transfer(sync_rs422tx_channel_t *channel, uint8_t *senddata[4], size_t len, size_t bits_len, uint32_t timeout_ms,uint32_t frame_num)
{
    int ret = 0;

    (void)timeout_ms;
    spi_pl_bus_select(channel->spi_bus, 0);
    bnc_channel_config_num(channel->bnc_channel, frame_num);
    spi_pl_bus_set_transmit_bits_len(channel->spi_bus, bits_len);
	// spi_pl_bus_reset_fifo(channel->spi_bus);
    ret = spi_pl_bus_write(channel->spi_bus, senddata, len);
    if (ret)
    {
        spi_pl_bus_start_transfer(channel->spi_bus);
    }
    return ret;
}

int sync_rs422tx_channel_write(sync_rs422tx_channel_t *channel, uint8_t *senddata[4], size_t len, size_t bits_len,uint32_t frame_num)
{
    int ret = 0;

    spi_pl_bus_select(channel->spi_bus, 0);
    bnc_channel_config_num(channel->bnc_channel, frame_num);
    spi_pl_bus_set_transmit_bits_len(channel->spi_bus, bits_len);
//	spi_pl_bus_reset_fifo(channel->spi_bus);
    ret = spi_pl_bus_write(channel->spi_bus, senddata, len);
    return ret;
}

void sync_rs422tx_channel_start_transfer(sync_rs422tx_channel_t *channel)
{
    spi_pl_bus_start_transfer(channel->spi_bus);
}


bool sync_rs422tx_channel_is_opened(sync_rs422tx_channel_t *channel)
{
    return (channel != NULL) && (channel->recv_thread != NULL);
}

int parse_sync_rs422tx_channel_config_from_file(int channel_id, const char *file_name, spi_pl_config_t *config, pulse_Config *tr_config)
{
    size_t file_size = 0;
    cJSON *json = NULL;
    cJSON *channels = NULL;
    cJSON *channel = NULL;
    int i = 0;
    int channel_count;
    char *content = NULL;
    const char *parse_error = NULL;

    if (config == NULL || file_name == NULL)
    {
        log_error("Invalid parameters");
        return -1;
    }

    content = vfs_read_content(file_name, &file_size);
    if (content == NULL)
    {
        log_error("Failed to read file %s", file_name);
        return -1;
    }
    json = cJSON_ParseWithLengthOpts(content, file_size, &parse_error, 0);
    vPortFree(content);
    if (json == NULL)
    {
        log_error("Failed to parse JSON from file %s, error near:%s, free_heap:%zu",
                  file_name, parse_error ? parse_error : "unknown", xPortGetFreeHeapSize());
        return -1;
    }

    channels = cJSON_GetObjectItem(json, "SYNCRS422TXchannels");
    if (!cJSON_IsArray(channels))
    {
        log_error("SYNCRS422TXchannels is not an array in file %s", file_name);
        cJSON_Delete(json);
        return -1;
    }

    channel_count = cJSON_GetArraySize(channels);
    for (i = 0; i < channel_count; i++)
    {
        channel = cJSON_GetArrayItem(channels, i);
        if (channel == NULL)
        {
            continue;
        }
        cJSON *id = cJSON_GetObjectItem(channel, "channel_id");
        if (cJSON_IsNumber(id) && id->valueint == channel_id)
        {
            break;
        }
    }

    if (i >= channel_count)
    {
        log_error("Failed to find channel%d config in file %s", channel_id, file_name);
        cJSON_Delete(json);
        return -1;
    }

    // 解析通道配置参数
    cJSON *clk_mode = cJSON_GetObjectItem(channel, "clk_mode");
    cJSON *cpha = cJSON_GetObjectItem(channel, "CPHA");
    cJSON *baudrate = cJSON_GetObjectItem(channel, "baudrate");
    cJSON *msb = cJSON_GetObjectItem(channel, "msb");
    cJSON *self_loop = cJSON_GetObjectItem(channel, "self_loop");
    cJSON *lock = cJSON_GetObjectItem(channel, "lock");

    // 设置配置参数
    config->Wait_Clk = cJSON_IsNumber(clk_mode) ? (u8)clk_mode->valueint : 0;
    config->Sample_Edge_Sel = cJSON_IsNumber(cpha) ? (u8)cpha->valueint : 0;
    config->BaudRate = cJSON_IsNumber(baudrate) ? (u32)baudrate->valueint : 4000000;
    config->First_Bit = cJSON_IsNumber(msb) ? (u8)msb->valueint : 0;
    config->self_loop = cJSON_IsTrue(self_loop) ? true : false;

    // 解析 lock 数组 [延时, 脉宽, 极性]
    if (cJSON_IsArray(lock) && cJSON_GetArraySize(lock) >= 3)
    {
        cJSON *delay = cJSON_GetArrayItem(lock, 0);
        cJSON *pulse_width = cJSON_GetArrayItem(lock, 1);
        cJSON *polarity = cJSON_GetArrayItem(lock, 2);

        config->Delay = cJSON_IsNumber(delay) ? (u32)delay->valuedouble : 10;
        config->Pulse_Width = cJSON_IsNumber(pulse_width) ? (u32)pulse_width->valuedouble : 1;
        config->Polarity = cJSON_IsNumber(polarity) ? (u8)polarity->valueint : 1;
    }


    // 解析 TR 配置参数
    cJSON *tr_count = cJSON_GetObjectItem(channel, "TR_count");
    cJSON *tr_mode = cJSON_GetObjectItem(channel, "TR_mode");
    cJSON *tr_delay = cJSON_GetObjectItem(channel, "TR_delay");
    cJSON *tr_tparam = cJSON_GetObjectItem(channel, "TR_Tparam");
    cJSON *tr_H_En = cJSON_GetObjectItem(channel, "TR_H_EN");
    cJSON *tr_V_En = cJSON_GetObjectItem(channel, "TR_V_EN");
    // 设置 TR 配置参数
    if (tr_config != NULL) {
        tr_config->Pulse_Num = cJSON_IsNumber(tr_count) ? (u32)tr_count->valueint : 100000;
        tr_config->Mode = cJSON_IsNumber(tr_mode) ? (u8)tr_mode->valueint : 2;
        tr_config->Trig_Delay = cJSON_IsNumber(tr_delay) ? (u32)tr_delay->valuedouble : 500;
        tr_config->TR_H_EN = cJSON_IsTrue(tr_H_En) ? true : false;
        tr_config->TR_V_EN = cJSON_IsTrue(tr_V_En) ? true : false;
        if (cJSON_IsArray(tr_tparam) && cJSON_GetArraySize(tr_tparam) >= 4)
        {
            cJSON *t0 = cJSON_GetArrayItem(tr_tparam, 0);
            cJSON *t1 = cJSON_GetArrayItem(tr_tparam, 1);
            cJSON *t2 = cJSON_GetArrayItem(tr_tparam, 2);
            cJSON *t3 = cJSON_GetArrayItem(tr_tparam, 3);

            tr_config->Delay = cJSON_IsNumber(t0) ? (u32)t0->valuedouble : 1;
            tr_config->Pulse_Width = cJSON_IsNumber(t1) ? (u32)t1->valuedouble : 2;
            tr_config->Period = cJSON_IsNumber(t2) ? (u32)t2->valuedouble : 6;
            tr_config->Safe_Time = cJSON_IsNumber(t3) ? (u32)t3->valuedouble : 1;
        }
    }

    cJSON_Delete(json);
    return 0;
}

// Register the channel event callback.
void sync_rs422tx_channel_register_event_callback(sync_rs422tx_channel_t *channel, void (*callback)(void *handle, u32 event), void *user_data)
{
    channel->event_callback = callback;
    channel->user_data = user_data;
}
