#ifndef __TEST_CASE_H__
#define __TEST_CASE_H__


#include <stdint.h>
#include <stdbool.h>
#include "sync_rs422tx_channel.h"
#include "bnc_channel.h"
#define TEST_CASE_MAX_CHANNELS SYNC_RS422TX_CHANNEL_NUM

// 测试状态
typedef enum
{
    TEST_STATE_IDLE = 0,
    TEST_STATE_RUNNING,
    TEST_STATE_PAUSED,
    TEST_STATE_STOPPED
} test_state_t;

//测试通道信息
typedef struct {
    int channel_id;
    bool valid;
    char *file_name;
    size_t file_size;
    size_t chunk_size;
    size_t chunk_bytes;
    int chunk_count;
    int current_chunk_index;
    uint8_t *file_data;
    uint32_t error_code;
} test_sync_rs422tx_channel_info_t;

typedef struct {
    int channel_id;
    uint32_t in1_cnt;
    uint32_t out1_cnt;
    uint32_t error_code;
} test_bnc_channel_info_t;

typedef struct {
    char *case_file_name;
    test_state_t state;
    int test_cycles;
    int current_cycle;
    test_bnc_channel_info_t bnc_info;
    test_sync_rs422tx_channel_info_t channels_info[TEST_CASE_MAX_CHANNELS];
} test_case_info_t;


test_case_info_t test_case_get_info(void);
char *test_case_get_info_str(void);
int test_case_init(const char *case_file, const char *md5);
int test_case_stop(void);
int test_case_start(void);
int test_case_pause(void);
int test_case_resume(void);


#endif /* __TEST_CASE_H__ */
