#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "vfs.h"
#include "env.h"
#include "cJSON.h"
#include "log.h"
#include "test_case.h"

#define TEST_CASE_MIN_CHUNK_COUNT 4
// 测试通道属性
typedef struct {
    int channel_id;
    bool valid;
    char file_name[32];
    size_t file_size;
    char md5[33];
    size_t chunk_size;
    size_t chunk_bytes;
    uint32_t frame_intervalms;
    int chunk_count;
    int current_chunk_index;   // 计算发送进度使用
    uint8_t *file_data;
    sync_rs422tx_channel_t *channel_handle;
    uint32_t error_code;
} test_sync_rs422tx_channel_t;

// 测试用例管理结构
typedef struct {
    test_state_t state;           // 状态（仅用于读取）
    bnc_channel_t *bnc_channel;
    int test_cycles;
    int current_cycle;
    bool file_mode;
    test_sync_rs422tx_channel_t channels[TEST_CASE_MAX_CHANNELS];
    TaskHandle_t test_task;
    EventGroupHandle_t control_events;  // 控制事件集
    test_case_info_t last_test_info;  // 上一次测试结果
} test_case_manager_t;


#define TEST_DEFAULT_CASE_CONFIG() {   \
    .state = TEST_STATE_IDLE,        \
    .test_cycles = 1,                  \
    .current_cycle = 0,                \
    .test_task = NULL,                 \
    .control_events = NULL,            \
}

// 控制事件定义
#define TEST_EVENT_START    (1 << 0)
#define TEST_EVENT_PAUSE    (1 << 1)
#define TEST_EVENT_RESUME   (1 << 2)
#define TEST_EVENT_STOP     (1 << 3)
#define TEST_EVENT_BNC_TRIGGER (1 << 4)

#define TEST_EVENT_CHANNEL_DONE_BASE 8

static test_case_manager_t g_test_manager = TEST_DEFAULT_CASE_CONFIG();

static EventBits_t test_case_get_channel_done_bit(int channel_id)
{
    if (channel_id < 0 || channel_id >= TEST_CASE_MAX_CHANNELS)
    {
        return 0;
    }
    return (EventBits_t)(1UL << (TEST_EVENT_CHANNEL_DONE_BASE + channel_id));
}

static TickType_t test_case_timeout_ms_to_ticks(uint32_t timeout_ms);

static int test_case_wait_for_event(test_case_manager_t *manager, EventBits_t wait_bits, uint32_t timeout_ms)
{
    EventBits_t event_bits = 0;
    TickType_t timeout_ticks = test_case_timeout_ms_to_ticks(timeout_ms);

    if (manager == NULL || manager->control_events == NULL)
    {
        return -1;
    }

    while (1)
    {
        event_bits = xEventGroupWaitBits(manager->control_events,
                                         wait_bits | TEST_EVENT_STOP | TEST_EVENT_PAUSE,
                                         pdFALSE,
                                         pdFALSE,
                                         timeout_ticks);
        if (event_bits == 0)
        {
            return -1;
        }

        if (event_bits & TEST_EVENT_STOP)
        {
            xEventGroupClearBits(manager->control_events, TEST_EVENT_STOP);
            manager->state = TEST_STATE_STOPPED;
            log_info("Test stopped");
            return 1;
        }

        if (event_bits & TEST_EVENT_PAUSE)
        {
            xEventGroupClearBits(manager->control_events, TEST_EVENT_PAUSE);
            manager->state = TEST_STATE_PAUSED;
            log_info("Test paused");
            event_bits = xEventGroupWaitBits(manager->control_events, TEST_EVENT_STOP | TEST_EVENT_RESUME, pdFALSE, pdFALSE, portMAX_DELAY);
            if (event_bits & TEST_EVENT_STOP)
            {
                xEventGroupClearBits(manager->control_events, TEST_EVENT_STOP);
                manager->state = TEST_STATE_STOPPED;
                log_info("Test stopped");
                return 1;
            }
            if (event_bits & TEST_EVENT_RESUME)
            {
                xEventGroupClearBits(manager->control_events, TEST_EVENT_RESUME);
                manager->state = TEST_STATE_RUNNING;
                log_info("Test resumed");
                continue;
            }
        }
        if (event_bits & wait_bits)
        {
            xEventGroupClearBits(manager->control_events, wait_bits);
            return 0;
        }
    }
}


static int test_case_prepare_current_chunk(test_sync_rs422tx_channel_t *channel)
{
    uint8_t *data[4] = {NULL};

    configASSERT(channel != NULL);
    configASSERT(channel->file_data != NULL);
    configASSERT(channel->channel_handle != NULL);
    if (!sync_rs422tx_channel_is_opened(channel->channel_handle))
    {
        log_error("Sync RS422TX channel %d is not opened", channel->channel_handle->id);
        return -1;
    }
    if ((channel->current_chunk_index < 0) || (channel->current_chunk_index + TEST_CASE_MIN_CHUNK_COUNT) > channel->chunk_count)
    {
        return -1;
    }
    uint32_t offset = channel->current_chunk_index * channel->chunk_bytes;
    data[0] = &channel->file_data[offset];
    data[1] = &channel->file_data[offset + channel->chunk_bytes];
    data[2] = &channel->file_data[offset + 2*channel->chunk_bytes];
    data[3] = &channel->file_data[offset + 3*channel->chunk_bytes];

    if (sync_rs422tx_channel_write(channel->channel_handle,
                                data,
                                channel->chunk_bytes,
                                channel->chunk_size,
                                1) != (int)channel->chunk_bytes)
    {
        return -1;
    }

    return 0;
}

static TickType_t test_case_timeout_ms_to_ticks(uint32_t timeout_ms)
{
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    if (timeout_ticks == 0 && timeout_ms > 0)
    {
        timeout_ticks = 1;
    }

    return timeout_ticks;
}

static uint32_t test_case_calculate_channel_timeout_ms(const test_sync_rs422tx_channel_t *channel)
{
    spi_pl_config_t spi_config = {0};
    spi_pl_config_t default_spi_config = DEFAULT_SPI_PL_BUS_CONFIG();
    uint64_t timeout_ms = 0;

    configASSERT(channel != NULL);
    configASSERT(channel->channel_handle != NULL);
    configASSERT(channel->channel_handle->spi_bus != NULL);

    spi_pl_bus_get_config(channel->channel_handle->spi_bus, &spi_config);
    if (spi_config.BaudRate == 0)
    {
        spi_config.BaudRate = default_spi_config.BaudRate;
    }

    timeout_ms = ((uint64_t)channel->chunk_size * 1000ULL) / spi_config.BaudRate;
    timeout_ms += 10ULL;
    if (timeout_ms > UINT32_MAX)
    {
        timeout_ms = UINT32_MAX;
    }

    return (uint32_t)(timeout_ms  + channel->frame_intervalms);
}

static uint32_t test_case_get_parallel_round_timeout_ms(test_case_manager_t *manager)
{
    uint32_t timeout_ms = 0;
    configASSERT(manager != NULL);
    for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
    {
        test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
        if (test_channel && test_channel->valid)
        {
            uint32_t channel_timeout_ms = 0;
//            channel_timeout_ms = test_case_calculate_channel_timeout_ms(test_channel);
            channel_timeout_ms = test_channel->frame_intervalms;
            if (channel_timeout_ms > timeout_ms)
            {
                timeout_ms = channel_timeout_ms;
            }
        }
    }

    return timeout_ms;
}

static bool test_case_validate_channels_parallel_param(test_case_manager_t *manager)
{
    const test_sync_rs422tx_channel_t *base_channel = NULL;
    configASSERT(manager != NULL);
    for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
    {
        test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
        if (test_channel && test_channel->valid)
        {
            if (base_channel == NULL)
            {
                base_channel = test_channel;
                continue;
            }
            if (test_channel->file_size != base_channel->file_size ||
                test_channel->chunk_size != base_channel->chunk_size ||
                test_channel->chunk_count != base_channel->chunk_count)
            {
                log_error("parallel mode requires same file_size/chunk_size/chunk_count across all valid channels, channel %d mismatch with channel %d",
                        test_channel->channel_id, base_channel->channel_id);
                return false;
            }
        }
    }
    return true;
}

static void test_bnc_event_callback(void *handle, u32 event)
{
    test_case_manager_t *manager = (test_case_manager_t *)handle;

    if (manager == NULL || manager->control_events == NULL)
    {
        return;
    }

    if (event == BNC_CHANNEL_NOTIFY_TRIGGER_TR)
    {
        xEventGroupSetBits(manager->control_events, TEST_EVENT_BNC_TRIGGER);
    }
}

/**
 * @brief 读取测试用例配置文件
 */
static int test_case_load_test_config_file(test_case_manager_t *test_manager, const char *file_name, const char *expected_md5)
{
    cJSON *json = NULL;
    cJSON *channels = NULL;
    cJSON *channel = NULL;
    int channel_count;
    const char *parse_error = NULL;
    char *content = NULL;
    size_t content_size = 0;
    uint8_t actual_md5[16] = {0};
    char actual_md5_str[33] = {0};
    // 读取文件
    int ret = vfs_read_content_with_md5(file_name, &content, &content_size, actual_md5);
    if (ret != 0 || content == NULL || content_size == 0U)
    {
        log_error("Failed to read test config file %s", file_name);
        return -1;
    }
    for (int i = 0; i < 16; i++)
    {
        sprintf(&actual_md5_str[i * 2], "%02x", actual_md5[i]);
    }
    if (strcmp(expected_md5, actual_md5_str) != 0)
    {
        log_error("Test config MD5 mismatch, expected: %s, actual: %s",
                  expected_md5, actual_md5_str);
        vPortFree(content);
        return -1;
    }

    // 解析JSON
    json = cJSON_ParseWithLengthOpts(content, content_size, &parse_error, 0);
    vPortFree(content);
    if (json == NULL)
    {
        log_error("Failed to parse JSON from file %s, error near:%s, free_heap:%zu",
                  file_name, parse_error ? parse_error : "unknown", xPortGetFreeHeapSize());
        return -1;
    }
    // 读取测试循环次数
    cJSON *test_cycles_json = cJSON_GetObjectItem(json, "testcycles");
    test_manager->test_cycles = cJSON_IsNumber(test_cycles_json) ? test_cycles_json->valueint : 1;
    if(test_manager->test_cycles <  0) {
        cJSON_Delete(json);
        log_error("testcycles invalid %d ,must be greater than 0", test_manager->test_cycles);
        return -1;
    }
    // 获取 SYNCRS422TXchannels 数组
    channels = cJSON_GetObjectItem(json, "SYNCRS422TXchannels");
    if (!cJSON_IsArray(channels)) {
        cJSON_Delete(json);
        log_error("SYNCRS422TXchannels is not an array in file %s", file_name);
        return -1;
    }
    // 获取文件模式
    cJSON *file_mode = cJSON_GetObjectItemCaseSensitive(json, "file_mode");
    test_manager->file_mode = cJSON_IsTrue(file_mode);
    // 遍历通道配置
    memset(test_manager->channels, 0, sizeof(test_manager->channels));
    channel_count = cJSON_GetArraySize(channels);
    if((channel_count > TEST_CASE_MAX_CHANNELS) || (channel_count == 0) )
    {
        cJSON_Delete(json);
        log_error("SYNCRS422TXchannels count %d > max", channel_count);
        return -1;
    }
    for (int i = 0; i < channel_count; i++) {
        channel = cJSON_GetArrayItem(channels, i);
        if (channel == NULL) {
            continue;
        }
        // 读取通道ID
        test_sync_rs422tx_channel_t *test_channel = &test_manager->channels[i];
        cJSON *id = cJSON_GetObjectItem(channel, "channel_id");
        if (!cJSON_IsNumber(id)) {
            continue;
        }

        int channel_id = id->valueint;
        if(channel_id < 0 || channel_id >= TEST_CASE_MAX_CHANNELS) {
            log_warn("Invalid channel_id %d , skipping", channel_id);
            continue;
        }        
        // 读取基本配置

        cJSON *file_name_json = cJSON_GetObjectItem(channel, "file_name");
        cJSON *md5_json = cJSON_GetObjectItem(channel, "md5");
        cJSON *chunk_size_json = cJSON_GetObjectItem(channel, "chunk_size");
        cJSON *frame_intervalms_json = cJSON_GetObjectItem(channel, "frame_interval");
        
        //读取通道相关配置
        test_channel->channel_id = channel_id;
        strncpy(test_channel->file_name, cJSON_IsString(file_name_json) ? file_name_json->valuestring : "", sizeof(test_channel->file_name) - 1);
        strncpy(test_channel->md5, cJSON_IsString(md5_json) ? md5_json->valuestring : "", sizeof(test_channel->md5) - 1);
        test_channel->chunk_size = cJSON_IsNumber(chunk_size_json) ? (uint32_t)chunk_size_json->valueint : 0;
        test_channel->frame_intervalms = cJSON_IsNumber(frame_intervalms_json) ? (uint32_t)frame_intervalms_json->valueint : 0;
        test_channel->file_data = NULL;
        test_channel->valid  = true;
    }

    cJSON_Delete(json);

    return 0;
}

// 卸载测试通道文件数据
static void test_case_unload_sync_rs422tx_channel_file(test_sync_rs422tx_channel_t *channel)
{
    if (channel->file_data)
    {
        vPortFree(channel->file_data);
        channel->file_data = NULL;
    }
    channel->file_size = 0;
    memset(channel->file_name, 0, sizeof(channel->file_name));
    memset(channel->md5, 0, sizeof(channel->md5));
    channel->chunk_count = 0;
    channel->chunk_bytes = 0;
    channel->current_chunk_index = 0;
    channel->valid = false;
}
// 加载测试通道文件数据
static int test_case_load_sync_rs422tx_channel_file(test_sync_rs422tx_channel_t *channel)
{
    uint8_t md5[16] = {0};
    char *content = NULL;
    char md5_str[33] = {0};

    if (vfs_read_content_with_md5(channel->file_name, &content,
                                  &channel->file_size, md5) != 0 ||
        content == NULL)
    {
        log_error("Failed to read file %s", channel->file_name);
        return -1;
    }
    for (int i = 0; i < 16; i++)
    {
        sprintf(&md5_str[i * 2], "%02x", md5[i]);
    }
    if (strcmp(channel->md5, md5_str) != 0)
    {
        log_error("Channel %d, MD5 mismatch for file %s, expected: %s, actual: %s",
                  channel->channel_id, channel->file_name, channel->md5, md5_str);
        vPortFree(content);
        return -1;
    }
    channel->file_data = (uint8_t *)content;
    if (channel->file_data  && channel->file_size > 0)
    {
        log_info("Loaded channel %d file %s, size: %zu bytes", channel->channel_id, channel->file_name, channel->file_size);
        // if (channel->file_size  <= (SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE * 4)) 
        {
            // 计算一个 data 码片大小（字节）
            channel->chunk_bytes = ((channel->chunk_size & 0x07) != 0) ? (channel->chunk_size / 8) + 1 : (channel->chunk_size / 8);
            if ((channel->chunk_bytes != 0) && (channel->chunk_bytes <= SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE)) // 码片长度是否合法
            {
                // 计算文件中包含多少个码片
                if (channel->file_size % channel->chunk_bytes == 0)
                {
                    channel->chunk_count = channel->file_size / channel->chunk_bytes;
                    if (channel->chunk_count >= TEST_CASE_MIN_CHUNK_COUNT)
                    {
                        // 检查码片个数
                        if (channel->chunk_count % TEST_CASE_MIN_CHUNK_COUNT == 0)
                        {
                            channel->valid = true;
                            return 0;
                        }
                        else
                        {
                            log_error("test case channel %d, chunk count %d is not a multiple of min chunk count %d", channel->channel_id, channel->chunk_count, TEST_CASE_MIN_CHUNK_COUNT);
                        }
                    }
                    else
                    {
                        log_error("test case channel %d, chunk count %d is less than min chunk count %d", channel->channel_id, channel->chunk_count, TEST_CASE_MIN_CHUNK_COUNT);
                    }
                }
                else
                {
                    log_error("test case channel %d, file size %zu is not a multiple of chunk bytes %zu", channel->channel_id, channel->file_size, channel->chunk_bytes);
                }
            }
            else
            {
                log_error("test case channel %d, chunk bytes %zu is invalid, min is %zu, max is %zu", channel->channel_id, channel->chunk_bytes, 1, SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE);
            }
        }
        // else
        // {
        //     log_error("test case channel %d, file size %zu is greater than max frame size %zu x4", channel->channel_id, channel->file_size, SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE);
        // }
    }
    else
    {
        log_error("Failed to load channel %d file %s", channel->channel_id, channel->file_name);
    }
    test_case_unload_sync_rs422tx_channel_file(channel);
    return -1;
}

/**
 * @brief 发送码片数据
 */
static int send_file_chunk(test_case_manager_t *manager, test_sync_rs422tx_channel_t *channel)
{
    EventBits_t done_bit = 0;

    configASSERT(manager != NULL);
	configASSERT(channel != NULL);
    configASSERT(channel->file_data != NULL);
    configASSERT(channel->channel_handle != NULL);

    channel->current_chunk_index = 0;
    done_bit = test_case_get_channel_done_bit(channel->channel_id);
//  uint32_t  timeout_ms = test_case_calculate_channel_timeout_ms(channel);
    int ret = 0;
    while (channel->current_chunk_index < channel->chunk_count)
    {
        ret = test_case_prepare_current_chunk(channel);
        if (ret == 0)
        {
            bnc_channel_enable(manager->bnc_channel);
            sync_rs422tx_channel_start_transfer(channel->channel_handle);
        }
        else
        {
            log_error("test case channel %d, prepare chunk %d failed", channel->channel_id, channel->current_chunk_index);
            break;
        }
        ret = test_case_wait_for_event(manager, done_bit, portMAX_DELAY);
        if(ret == 0)
        {
            channel->current_chunk_index += TEST_CASE_MIN_CHUNK_COUNT;
            if (channel->frame_intervalms)
            {
                vTaskDelay(pdMS_TO_TICKS(channel->frame_intervalms));
            }
            else
            {
            	vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        else
        {
            if(ret == -1)
            {
                log_error("test case channel %d, send chunk %d timeout", channel->channel_id, channel->current_chunk_index);
            }
            break;
        }
    }
    return ret;
}

/**
 * @brief 测试事件回调函数
 */
static void test_event_callback(void *handle, u32 event) {
    test_sync_rs422tx_channel_t *test_channel = (test_sync_rs422tx_channel_t *)handle;
    configASSERT(test_channel != NULL);
    if (event == SYNC_RS422TX_NOTIFY_SPI_DONE)
    {
        EventBits_t done_bit = test_case_get_channel_done_bit(test_channel->channel_id);
        if (done_bit != 0 && g_test_manager.control_events)
        {
            xEventGroupSetBits(g_test_manager.control_events, done_bit);
        }
    }
}

//打开RS422测试通道
static int  test_case_manager_open_sync_rs422tx_channels(test_case_manager_t *manager)
{
    for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
    {
        test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
        if (test_channel->valid)
        {
            test_channel->channel_handle = sync_rs422tx_channel_get_handle(test_channel->channel_id);
            if (test_channel->channel_handle)
            {
                sync_rs422tx_channel_open(test_channel->channel_handle);
                sync_rs422tx_channel_register_event_callback(test_channel->channel_handle, test_event_callback, test_channel);
            }
        }
    }
    return 0;
}

// 关闭RS422测试通道
static void test_case_manager_close_sync_rs422tx_channels(test_case_manager_t *manager)
{
    for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
    {
        test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
        if(test_channel->valid && test_channel->channel_handle) {
            sync_rs422tx_channel_register_event_callback(test_channel->channel_handle, NULL, NULL);
            sync_rs422tx_channel_close(test_channel->channel_handle);
        }
    }
}

// 串行发送流程处理
void test_case_manager_send_sequential(test_case_manager_t *manager)
{
    manager->current_cycle = 0;
    while (1)
    {
        configASSERT(manager != NULL);
        for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
        {
            test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
            if (test_channel->valid && test_channel->channel_handle)
            {
                int ret = send_file_chunk(manager, test_channel);
                if (ret == 0)
                {
                    log_info("test case cycles(%d) channel %d, send file %s successfully", manager->current_cycle, test_channel->channel_id, test_channel->file_name);
                    continue;
                }
                else
                {
                    test_channel->error_code = ret;
                    return;
                }
            }
        }
        manager->current_cycle++;
        log_debug("sequential send cycle %d", manager->current_cycle);
        if (manager->test_cycles != 0 && manager->current_cycle >= manager->test_cycles)
        {
            break;
        }
    }

}
// Split file_data into four lanes in data0, data1, data2, data3 order.
int test_case_prepare_file(test_sync_rs422tx_channel_t *test_channel)
{
    static uint8_t data[TEST_CASE_MIN_CHUNK_COUNT][SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE];
    uint8_t *data_ptr[TEST_CASE_MIN_CHUNK_COUNT] = {data[0], data[1], data[2], data[3]};
    uint32_t frame_num;
    size_t data_len;
    size_t frame_index;
    int ret;

    configASSERT(test_channel != NULL);
    configASSERT(test_channel->file_data != NULL);
    configASSERT(test_channel->channel_handle != NULL);

    if (!sync_rs422tx_channel_is_opened(test_channel->channel_handle) ||
        test_channel->chunk_bytes == 0 ||
        test_channel->chunk_count < TEST_CASE_MIN_CHUNK_COUNT ||
        (test_channel->chunk_count % TEST_CASE_MIN_CHUNK_COUNT) != 0 ||
        test_channel->file_size !=
            ((size_t)test_channel->chunk_count * test_channel->chunk_bytes))
    {
        return -1;
    }

    frame_num = (uint32_t)(test_channel->chunk_count / TEST_CASE_MIN_CHUNK_COUNT);
    data_len = (size_t)frame_num * test_channel->chunk_bytes;
    if (test_channel->chunk_bytes > SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE ||
        data_len > SYNC_RS422TX_CHANNEL_MAX_FRAME_SIZE)
    {
        return -1;
    }

    for (frame_index = 0; frame_index < frame_num; frame_index++)
    {
        size_t source_offset = frame_index * TEST_CASE_MIN_CHUNK_COUNT * test_channel->chunk_bytes;
        size_t destination_offset = frame_index * test_channel->chunk_bytes;

        memcpy(&data[0][destination_offset],
               &test_channel->file_data[source_offset],
               test_channel->chunk_bytes);
        memcpy(&data[1][destination_offset],
               &test_channel->file_data[source_offset + test_channel->chunk_bytes],
               test_channel->chunk_bytes);
        memcpy(&data[2][destination_offset],
               &test_channel->file_data[source_offset + 2 * test_channel->chunk_bytes],
               test_channel->chunk_bytes);
        memcpy(&data[3][destination_offset],
               &test_channel->file_data[source_offset + 3 * test_channel->chunk_bytes],
               test_channel->chunk_bytes);
    }

    ret = sync_rs422tx_channel_write(test_channel->channel_handle,
                                     data_ptr,
                                     data_len,
                                     test_channel->chunk_size,
                                     frame_num);
    if (ret != (int)data_len)
    {
//        log_error("test case channel %d, prepare chunk failed", test_channel->channel_id);
        return -1;
    }
    test_channel->current_chunk_index  = 0;
    return 0;
}

void test_case_manager_send_parallel_file_mode(test_case_manager_t *manager)
{
    configASSERT(manager != NULL);
    manager->current_cycle = 0;
    int ret = 0;
    uint32_t timeout_ms = test_case_get_parallel_round_timeout_ms(manager);
    uint8_t mask = 0;

// 准备选中通道的文件数据
    for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
    {
        test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
        if (test_channel && test_channel->valid && test_channel->channel_handle)
        {
            ret = test_case_prepare_file(test_channel);
            if (ret != 0)
            {
                test_channel->error_code = ret;
                log_error("test case channel %d, prepare file(%s) failed", test_channel->channel_id,test_channel->file_name);
                return;
            }
        }
    }

    /* Parallel mode uses the BNC trigger as the batch-level completion barrier. */
    bnc_channel_in1_enable(manager->bnc_channel);
    bnc_channel_start(manager->bnc_channel);//兼容桌面模式
    while(1)
    {
        ret = test_case_wait_for_event(manager, TEST_EVENT_BNC_TRIGGER, portMAX_DELAY);
        if (ret == 0)
        {
            for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
            {
                test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
                if (test_channel && test_channel->valid && test_channel->channel_handle)
                {
                    test_channel->current_chunk_index += TEST_CASE_MIN_CHUNK_COUNT;
                    spi_pl_config_t config= {0};
                    spi_pl_bus_get_config(test_channel->channel_handle->spi_bus,&config);
                    if (config.self_loop) // debug loop
                    {
                        xTaskNotify(test_channel->channel_handle->recv_thread,
                                    SYNC_RS422TX_NOTIFY_SPI_DONE,
                                    eSetBits);
                    }
                    if (test_channel->current_chunk_index >= test_channel->chunk_count)
                    {
                        mask |= (1 << i);
                        test_channel->current_chunk_index = 0;
//                        log_info("test case cycles(%d) channel %d, send file %s successfully",
//                            manager->current_cycle, test_channel->channel_id, test_channel->file_name);
                    }
                }
            }
            if (mask) // 所有通道都发送完文件
            {
                manager->current_cycle++;
                mask = 0;
            }
        }
        else
        {
            break;
        }
        if (manager->test_cycles != 0 && manager->current_cycle >= manager->test_cycles)
        {
            break;
        }
    }
    bnc_channel_in1_disable(manager->bnc_channel);

}

void test_case_manager_send_parallel_chunk_mode(test_case_manager_t *manager)
{
    configASSERT(manager != NULL);
    manager->current_cycle = 0;
    int ret = 0;
    uint32_t prepare_start;
    uint32_t prepare_end1;
    uint32_t prepare_end2;
    while (1)
    {
        uint32_t timeout_ms = test_case_get_parallel_round_timeout_ms(manager);
        uint8_t mask = 0;
        while (1)
        {
            //prepare_start = portGET_RUN_TIME_COUNTER_VALUE();
            //准备每个通道的chunk数
            for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
            {
                test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
                if (test_channel && test_channel->valid && test_channel->channel_handle && test_channel->current_chunk_index < test_channel->chunk_count)
                {
                    ret = test_case_prepare_current_chunk(test_channel);
                    if (ret != 0)
                    {
                        test_channel->error_code = ret;
                        log_error("test case channel %d, prepare chunk %d failed", test_channel->channel_id, test_channel->current_chunk_index);
                        return;
                    }
                }
            }

            //            prepare_end1 = portGET_RUN_TIME_COUNTER_VALUE();
            /* Parallel mode uses the BNC trigger as the batch-level completion barrier. */
//            bnc_channel_start(manager->bnc_channel); //
            bnc_channel_in1_enable(manager->bnc_channel);
            ret = test_case_wait_for_event(manager, TEST_EVENT_BNC_TRIGGER, portMAX_DELAY);
            if (ret == 0)
            {
                bnc_channel_in1_disable(manager->bnc_channel);//防止bnc控制器接收噪声输入脉冲信号
                for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
                {
                    test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
                    if (test_channel && test_channel->valid && test_channel->channel_handle)
                    {
                        test_channel->current_chunk_index += TEST_CASE_MIN_CHUNK_COUNT;
                        spi_pl_config_t config= {0};
                        spi_pl_bus_get_config(test_channel->channel_handle->spi_bus,&config);
                        if (config.self_loop) // debug loop
                        {
                            xTaskNotify(test_channel->channel_handle->recv_thread,
                                        SYNC_RS422TX_NOTIFY_SPI_DONE,
                                        eSetBits);
                        }
                        if (test_channel->current_chunk_index >= test_channel->chunk_count)
                        {
                            mask |= (1 << i);
                            test_channel->current_chunk_index = 0;
                            //                            log_info("test case cycles(%d) channel %d, send file %s successfully",
                            //                                    manager->current_cycle, test_channel->channel_id, test_channel->file_name);
                        }
                    }
                }
                //                prepare_end2 = portGET_RUN_TIME_COUNTER_VALUE();
                if (timeout_ms)
                {
                    vTaskDelay(pdMS_TO_TICKS(timeout_ms));
                }
                if (mask) // 所有通道都发送完成
                {
                    break;
                }
            }
            else
            {
                if (ret == -1)
                {
                    log_error("parallel batch wait timeout");
                }
                return;
            }
        }
        manager->current_cycle++;
        //        log_debug("parallel send cycle %d, prepare time %lu ms, send time %lu ms",
        //                  manager->current_cycle,
        //                  (unsigned long)(prepare_end1 - prepare_start),
        //                  (unsigned long)(prepare_end2 - prepare_end1));
        //       log_debug("parallel send cycle %d", manager->current_cycle);
        if (manager->test_cycles != 0 && manager->current_cycle >= manager->test_cycles)
        {
            break;
        }
    }
}

static test_bnc_channel_info_t test_case_get_bnc_info(void)
{
    test_bnc_channel_info_t info = {0};
    if (g_test_manager.bnc_channel != NULL)
    {
        info.channel_id = g_test_manager.bnc_channel->id;
        info.in1_cnt = bnc_channel_get_in1_cnt(g_test_manager.bnc_channel);
        info.out1_cnt = bnc_channel_get_out1_cnt(g_test_manager.bnc_channel);
        info.error_code = 0;
    }
    return info;
}

static test_case_info_t test_case_get_result(test_case_manager_t *manager)
{
    test_case_info_t info = {0};
    info.case_file_name = getenv(TEST_CASE_FILE_NAME);
    info.state = manager->state;
    info.test_cycles = manager->test_cycles;
    info.current_cycle = manager->current_cycle;
    info.bnc_info = test_case_get_bnc_info();
    for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
    {
        if (manager->channels[i].valid)
        {
            info.channels_info[i].valid = manager->channels[i].valid;
            info.channels_info[i].channel_id = manager->channels[i].channel_id;
            info.channels_info[i].file_name = manager->channels[i].file_name;
            info.channels_info[i].file_size = manager->channels[i].file_size;
            info.channels_info[i].chunk_size = manager->channels[i].chunk_size;
            info.channels_info[i].chunk_bytes = manager->channels[i].chunk_bytes;
            info.channels_info[i].chunk_count = manager->channels[i].chunk_count;
            info.channels_info[i].current_chunk_index = manager->channels[i].current_chunk_index;
            info.channels_info[i].file_data = manager->channels[i].file_data;
            info.channels_info[i].error_code = manager->channels[i].error_code;
        }
    }
    return info;
}

/**
 * @brief 测试任务函数
 */
static void test_task_func(void *arg) {
    (void)arg;
    test_case_manager_t *manager = arg;
    configASSERT(manager != NULL);
    manager->control_events = xEventGroupCreate();
    configASSERT(manager->control_events != NULL);
    EventBits_t event_bits = xEventGroupWaitBits(manager->control_events,TEST_EVENT_START | TEST_EVENT_STOP, pdTRUE, pdFALSE, portMAX_DELAY);
    if (event_bits & TEST_EVENT_START) 
    {
        manager->state = TEST_STATE_RUNNING;
        log_info("Test started");
        manager->bnc_channel = bnc_channel_get_handle(0);
        configASSERT(manager->bnc_channel != NULL);
        bnc_channel_open(manager->bnc_channel);
        BNC_GEN_Config config = DEFAULT_BNC_GEN_CONFIG();
        bnc_channel_get_config(manager->bnc_channel, &config);
        bnc_channel_register_event_callback(manager->bnc_channel, test_bnc_event_callback, manager);

        // 加载通道绑定测试文件
        for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
        {
            test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
            if (test_channel && test_channel->valid)
            {
                int ret = test_case_load_sync_rs422tx_channel_file(test_channel); // 加载通道绑定文件
                if(ret != 0)
                {
                    goto exit;
                }
            }
        }
        if (config.mode == BNC_GEN_MODE_MASTER_PARALLEL || config.mode == BNC_GEN_MODE_SLAVE_PARALLEL)
        {
            // 并行模式下校验所有通道绑定的文件参数是否一致
            bool valid = test_case_validate_channels_parallel_param(manager);
            if (valid ==false)
            {
                goto exit;
            }
        }
        test_case_manager_open_sync_rs422tx_channels(manager);
        if (config.mode == BNC_GEN_MODE_MASTER_PARALLEL || config.mode == BNC_GEN_MODE_SLAVE_PARALLEL)
        {
            if(manager->file_mode)
                test_case_manager_send_parallel_file_mode(manager);
            else
                test_case_manager_send_parallel_chunk_mode(manager);
        }
        else
        {
            test_case_manager_send_sequential(manager);
        }
        manager->state = TEST_STATE_STOPPED;
        manager->last_test_info = test_case_get_result(manager); // 记录测试结果快照
        test_case_manager_close_sync_rs422tx_channels(manager);
    exit:
        bnc_channel_register_event_callback(manager->bnc_channel, NULL, NULL);
        bnc_channel_close(manager->bnc_channel);
        // 卸载通道绑定测试文件
        for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
        {
            test_sync_rs422tx_channel_t *test_channel = &manager->channels[i];
            if (test_channel && test_channel->valid)
            {
                test_case_unload_sync_rs422tx_channel_file(test_channel);
            }
        }
    }

    if (manager->control_events)
    {
        xEventGroupClearBits(manager->control_events, TEST_EVENT_START | TEST_EVENT_STOP | TEST_EVENT_PAUSE | TEST_EVENT_RESUME);
        vEventGroupDelete(manager->control_events);
        manager->control_events = NULL;
    }
    unsetenv(TEST_CASE_FILE_NAME);
	manager->state = TEST_STATE_IDLE;
	manager->test_task = NULL;
	log_info("Test finished");
	vTaskDelete(manager->test_task);
}

/**
 * @brief 初始化测试用例服务
 * @param case_file 测试用例配置文件名称
 */
int test_case_init(const char *case_file, const char *expected_md5)
{
    int ret;

    if (case_file == NULL || expected_md5 == NULL ||
        g_test_manager.state != TEST_STATE_IDLE)
    {
        log_error("Test case service is not idle or parameters are invalid");
        return -1;
    }
    ret = test_case_load_test_config_file(&g_test_manager, case_file, expected_md5);
    if (ret != 0)
    {
        log_error("Failed to load test config file %s", case_file);
        return -1;
    }
    setenv(TEST_CASE_FILE_NAME, case_file, 1);

    if (g_test_manager.test_task == NULL &&
        xTaskCreate(test_task_func, "test_task", configMINIMAL_STACK_SIZE * 3,
                    &g_test_manager, tskIDLE_PRIORITY + 6,
                    &g_test_manager.test_task) != pdPASS)
    {
        log_error("Failed to create test task");
        return -1;
    }
    return 0;
}

/**
 * @brief 启动测试
 */
int test_case_start(void) {
    if (g_test_manager.state == TEST_STATE_RUNNING) {
        log_warn("Test is already running");
        return 0;
    }

    // 发送启动事件
    if (g_test_manager.control_events)
        xEventGroupSetBits(g_test_manager.control_events, TEST_EVENT_START);

    return 0;
}

/**
 * @brief 暂停测试
 */
int test_case_pause(void) {
    if (g_test_manager.state != TEST_STATE_RUNNING) {
        log_warn("Test is not running");
        return 0;
    }

    // 发送暂停事件
    if (g_test_manager.control_events)
        xEventGroupSetBits(g_test_manager.control_events, TEST_EVENT_PAUSE);

    return 0;
}

/**
 * @brief 继续测试
 */
int test_case_resume(void) {
    if (g_test_manager.state != TEST_STATE_PAUSED) {
        log_warn("Test is not paused");
        return 0;
    }
    if (g_test_manager.control_events)
        // 发送恢复事件
        xEventGroupSetBits(g_test_manager.control_events, TEST_EVENT_RESUME);

    return 0;
}

/**
 * @brief 停止测试
 */
int test_case_stop(void) {

    if (g_test_manager.state == TEST_STATE_STOPPED) {
        log_warn("Test is not running");
        return 0;
    }
    if (g_test_manager.control_events)
        // 发送停止事件
        xEventGroupSetBits(g_test_manager.control_events, TEST_EVENT_STOP);
    return 0;
}


/**
 * @brief 获取当前测试用例信息
 * @return 测试用例信息结构
 */
test_case_info_t test_case_get_info(void)
{
    if(g_test_manager.state == TEST_STATE_RUNNING)
    {
        return test_case_get_result(&g_test_manager);
    }
    return g_test_manager.last_test_info;
}


char *test_case_get_info_str(void)
{
    char *json_str = NULL;
    test_case_info_t info = test_case_get_info();
    BNC_GEN_Config bnc_config = DEFAULT_BNC_GEN_CONFIG();

    if (g_test_manager.bnc_channel != NULL)
    {
        bnc_channel_get_config(g_test_manager.bnc_channel, &bnc_config);
    }

    /* Slave parallel mode only needs the test state and BNC counters. */
    if (bnc_config.mode == BNC_GEN_MODE_SLAVE_PARALLEL)
    {
        json_str = cJSON_malloc(160);
        if (json_str == NULL)
        {
            log_error("Failed to allocate compact test info string");
            return NULL;
        }

        int ret = snprintf(json_str, 160,
                           "{\"state\":\"%s\",\"current_cycle\":%d,\"BNC\":{\"channel_id\":%d,\"in1_cnt\":%lu,\"out1_cnt\":%lu}}",
                           info.state == TEST_STATE_IDLE ? "IDLE" :
                           info.state == TEST_STATE_RUNNING ? "RUNNING" :
                           info.state == TEST_STATE_PAUSED ? "PAUSED" :
                           info.state == TEST_STATE_STOPPED ? "STOPPED" : "UNKNOWN",
                           info.current_cycle,
                           info.bnc_info.channel_id,
                           (unsigned long)info.bnc_info.in1_cnt,
                           (unsigned long)info.bnc_info.out1_cnt);
        if (ret < 0 || ret >= 160)
        {
            cJSON_free(json_str);
            return NULL;
        }
        return json_str;
    }

    cJSON *root = cJSON_CreateObject();
    if(root == NULL)
    {
        log_error("Failed to create json object");
        return NULL;
    }
    const char *state_str = NULL;
    switch(info.state)
    {
        case TEST_STATE_IDLE:
            state_str = "IDLE";
            break;
        case TEST_STATE_RUNNING:
            state_str = "RUNNING";
            break;
        case TEST_STATE_PAUSED:
            state_str = "PAUSED";
            break;
        case TEST_STATE_STOPPED:
            state_str = "STOPPED";
            break;
        default:
            state_str = "UNKNOWN";
            break;
    }
    // 添加基本信息
    if (info.case_file_name != NULL)
    {
        cJSON_AddStringToObject(root, "case_file_name", info.case_file_name);
    }
    cJSON_AddStringToObject(root, "state", state_str);
    cJSON_AddNumberToObject(root, "test_cycles", info.test_cycles);
    cJSON_AddNumberToObject(root, "current_cycle", info.current_cycle);
    cJSON *bnc = cJSON_CreateObject();
    cJSON_AddNumberToObject(bnc, "channel_id", info.bnc_info.channel_id);
    cJSON_AddNumberToObject(bnc, "in1_cnt", info.bnc_info.in1_cnt);
    cJSON_AddNumberToObject(bnc, "out1_cnt", info.bnc_info.out1_cnt);
    cJSON_AddItemToObject(root, "BNC", bnc);
    // 添加通道信息数组
    cJSON *channels = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "SYNCRS422TXchannels", channels);
    for (int i = 0; i < TEST_CASE_MAX_CHANNELS; i++)
    {
        if(info.channels_info[i].valid)
        {
            cJSON *channel = cJSON_CreateObject();
            cJSON_AddNumberToObject(channel, "channel_id", info.channels_info[i].channel_id);
            cJSON_AddStringToObject(channel, "file_name",
                                    info.channels_info[i].file_name != NULL ? info.channels_info[i].file_name : "");
            cJSON_AddNumberToObject(channel, "file_size", info.channels_info[i].file_size);
            cJSON_AddNumberToObject(channel, "chunk_size", info.channels_info[i].chunk_size);
            cJSON_AddNumberToObject(channel, "chunk_count", info.channels_info[i].chunk_count);
            cJSON_AddNumberToObject(channel, "chunk_bytes", info.channels_info[i].chunk_bytes);
            cJSON_AddNumberToObject(channel, "current_chunk_index", info.channels_info[i].current_chunk_index);
            cJSON_AddNumberToObject(channel, "error_code", info.channels_info[i].error_code);
            cJSON_AddItemToArray(channels, channel);
        }
    }
    // 转换为字符串
    json_str = cJSON_Print(root);
    cJSON_Delete(root);
    return json_str;
}
