#include "bnc_channel.h"
#include "queue.h"
#include "message_buffer.h"
#include "pb_decode.h"
#include "dtu.pb.h"
#include "pb_encode.h"
#include "vfs.h"
#include "env.h"
#include "dtu.h"
#include "cJSON.h"
#include "log.h"

// #define BNC_CHANNEL_STATUS_ENABLE_BIT 0
#define PROTO_MSG_HEAD_SIZE 256
#define PROTO_SUB1MSG_SIZE (SubMsg1_size + PROTO_MSG_HEAD_SIZE)
#define PROTO_SUB2MSG_SIZE (SubMsg2_size + PROTO_MSG_HEAD_SIZE)

static void bnc_channel_event_callback(void *channel_handle, u32 event)
{
    bnc_channel_t *channel = (bnc_channel_t *)channel_handle;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    configASSERT(channel != NULL);
    configASSERT(channel->notify_task != NULL);

    if (event == BNC_GEN_EVENT_TRIGER_VNA)
    {
        xTaskNotifyFromISR(channel->notify_task, BNC_CHANNEL_NOTIFY_TRIGGER_VNA, eSetBits, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    else if (event == BNC_GEN_EVENT_TRIGER_TR)
    {
        xTaskNotifyFromISR(channel->notify_task, BNC_CHANNEL_NOTIFY_TRIGGER_TR, eSetBits, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static int bnc_channel_load_config_from_file(int channel_id, const char *file_name, BNC_GEN_Config *config)
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

    channels = cJSON_GetObjectItem(json, "BNC");
    if (!cJSON_IsArray(channels))
    {
        log_error("BNC is not an array in file %s", file_name);
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
        log_error("Failed to find BNC channel%d config in file %s", channel_id, file_name);
        cJSON_Delete(json);
        return -1;
    }

    cJSON *in1_polarity = cJSON_GetObjectItem(channel, "in1_polarity");
    cJSON *in2_polarity = cJSON_GetObjectItem(channel, "in2_polarity");
    cJSON *out1_polarity = cJSON_GetObjectItem(channel, "out1_polarity");
    cJSON *mode = cJSON_GetObjectItem(channel, "mode");

    config->in1_polarity = cJSON_IsNumber(in1_polarity) ? (u8)in1_polarity->valueint : 0;
    config->in2_polarity = cJSON_IsNumber(in2_polarity) ? (u8)in2_polarity->valueint : 0;
    config->out1_polarity = cJSON_IsNumber(out1_polarity) ? (u8)out1_polarity->valueint : 0;
    config->mode = cJSON_IsNumber(mode) ? (u8)mode->valueint : BNC_GEN_MODE_MASTER_SERIAL;

    cJSON_Delete(json);
    return 0;
}

int package_bnc_channel_resp_msg(bnc_channel_t *channel, uint32_t message_id, int error_code)
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
    msg->type = ChannelType_BNC;
    msg->which_values = OneOfMessage_submsg2_tag;
    gettimeofday(&tv, NULL);
//    long long milliseconds = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    long long milliseconds = 0;
    msg->id = milliseconds;
    msg->values.submsg2.timestamp = milliseconds;
    msg->values.submsg2.channel = channel->id;
    msg->values.submsg2.event_code = error_code;
    stream = pb_ostream_from_buffer(encode_buf, PROTO_SUB2MSG_SIZE);
    if (!pb_encode(&stream, OneOfMessage_fields, msg))
    {
        log_error("Encoding bnc channel %d resp frame failed: %s", channel->id, PB_GET_ERROR(&stream));
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

static void bnc_channel_recv_thread(void *arg)
{
    bnc_channel_t *channel = (bnc_channel_t *)arg;
    BNC_GEN_Config config = DEFAULT_BNC_GEN_CONFIG();
    const char *file_name = getenv(TEST_PARAM_FILE_NAME);
    configASSERT(channel != NULL);
    channel->notify_task = xTaskGetCurrentTaskHandle();

    if (file_name != NULL && bnc_channel_load_config_from_file(channel->id, file_name, &config) != 0)
    {
        log_warn("BNC channel %d load config from %s failed, keep current config", channel->id, file_name);
    }
    bnc_gen_dev_config(&config);
    bnc_gen_clr_cnt();
    bnc_gen_dev_bind_irq_callback(bnc_channel_event_callback, channel);
    bnc_gen_enable();
    // SET1_BIT(channel->status, BNC_CHANNEL_STATUS_ENABLE_BIT);
    log_info("BNC channel %d open success", channel->id);

    while (1)
    {
        uint32_t notify_value = 0;

        if (xTaskNotifyWait(0, 0xffffffffUL, &notify_value, portMAX_DELAY) != pdPASS)
        {
            continue;
        }
        if ((notify_value & BNC_CHANNEL_NOTIFY_CLOSE) != 0)
        {
            break;
        }
        if ((notify_value & BNC_CHANNEL_NOTIFY_TRIGGER_VNA) != 0)
        {
            if (channel->event_callback != NULL)
                channel->event_callback(channel->user_data, BNC_CHANNEL_NOTIFY_TRIGGER_VNA);
            package_bnc_channel_resp_msg(channel, 0, BNC_CHANNEL_NOTIFY_TRIGGER_VNA);
        }
        if ((notify_value & BNC_CHANNEL_NOTIFY_TRIGGER_TR) != 0)
        {
            if (channel->event_callback != NULL)
                channel->event_callback(channel->user_data, BNC_CHANNEL_NOTIFY_TRIGGER_TR);
        }
    }
    bnc_gen_dev_bind_irq_callback(NULL, NULL);
    bnc_gen_disable();
    bnc_gen_clr_cnt();
    channel->notify_task = NULL;
    // SET0_BIT(channel->status, BNC_CHANNEL_STATUS_ENABLE_BIT);
    channel->recv_thread = NULL;
    vTaskDelete(NULL);
}

static bnc_channel_t bnc_channel[BNC_CHANNEL_NUM] = {
    {
        .id = 0,
    },
};

void bnc_channel_init(void)
{
    bnc_gen_dev_init();
    for (int i = 0; i < sizeof(bnc_channel) / sizeof(bnc_channel[0]); i++)
    {
        log_info("BNC channel %d init success", bnc_channel[i].id);
    }
}

bnc_channel_t *bnc_channel_get_handle(int channel_id)
{
    int count = sizeof(bnc_channel) / sizeof(bnc_channel[0]);
    for (int i = 0; i < count; i++)
    {
        if (bnc_channel[i].id == channel_id)
        {
            return &bnc_channel[i];
        }
    }
    return NULL;
}

bool bnc_channel_is_opened(bnc_channel_t *channel)
{
    return channel != NULL && channel->recv_thread != NULL;
}

int bnc_channel_config(bnc_channel_t *channel, BNC_GEN_Config *config)
{
    if (channel == NULL || config == NULL)
    {
        return -1;
    }
    return bnc_gen_dev_config(config);
}

void bnc_channel_get_config(bnc_channel_t *channel, BNC_GEN_Config *config)
{
    if (channel == NULL)
    {
        return;
    }

    bnc_gen_dev_get_config(config);
}

int bnc_channel_open(bnc_channel_t *channel)
{
    if (bnc_channel_is_opened(channel))
    {
        // log_warn("BNC channel %d already opened", channel->id);
        return 0;
    }

    char thread_name[32];
    snprintf(thread_name, sizeof(thread_name), "bnc_recv_channel_%d", channel->id);
    channel->recv_thread = sys_thread_new(thread_name, bnc_channel_recv_thread, channel,
                                          configMINIMAL_STACK_SIZE * 2, tskIDLE_PRIORITY + 8);
    if (channel->recv_thread == NULL)
    {
        log_error("BNC channel %d create recv thread failed", channel->id);
        return -1;
    }
    return 0;
}

void bnc_channel_close(bnc_channel_t *channel)
{
    if (!bnc_channel_is_opened(channel))
    {
        // log_warn("BNC channel %d already closed", channel->id);
        return;
    }
    // bnc_channel_disable(channel);
    xTaskNotify(channel->recv_thread, BNC_CHANNEL_NOTIFY_CLOSE, eSetBits);
    while (channel->recv_thread != NULL)
    {
    	vTaskDelay(pdMS_TO_TICKS(1));
//        taskYIELD();
    }
    log_info("BNC channel %d closed", channel->id);
}

void bnc_channel_register_event_callback(bnc_channel_t *channel, void (*event_callback)(void *channel_handle, u32 event), void *user_data)
{
    channel->event_callback = event_callback;
    channel->user_data = user_data;
}

void bnc_channel_start(bnc_channel_t *channel)
{
    if (channel == NULL)
    {
        return;
    }

    bnc_gen_start();
}

void bnc_channel_disable(bnc_channel_t *channel)
{
    if (channel == NULL)
    {
        return;
    }

    bnc_gen_disable();
}

void bnc_channel_enable(bnc_channel_t *channel)
{
    if (channel == NULL)
    {
        return;
    }

    bnc_gen_enable();
}

u32 bnc_channel_get_in1_cnt(bnc_channel_t *channel)
{
    if (channel == NULL)
    {
        return 0;
    }

    return bnc_gen_get_in1_cnt();
}

u32 bnc_channel_get_out1_cnt(bnc_channel_t *channel)
{
    if (channel == NULL)
    {
        return 0;
    }

    return bnc_gen_get_out1_cnt();
}

void bnc_channel_config_num(bnc_channel_t *channel, u32 num)
{
    if (channel == NULL)
    {
        return;
    }
    return bnc_config_num(num);    
}

void bnc_channel_in1_enable(bnc_channel_t *channel)
{
    if (channel == NULL)
    {
        return;
    }

    bnc_gen_in1_enable();
}

void bnc_channel_in1_disable(bnc_channel_t *channel)
{
    if (channel == NULL)
    {
        return;
    }

    bnc_gen_in1_disable();
}
