#ifndef BNC_GEN_H
#define BNC_GEN_H


#ifdef __cplusplus
extern "C" {
#endif

/****************** Include Files ********************/
#include "xil_types.h"

typedef enum
{
    BNC_GEN_MODE_MASTER_PARALLEL = 0, // 桌面并行模式
    BNC_GEN_MODE_SLAVE_PARALLEL = 1,    // 暗室并行模式
    BNC_GEN_MODE_MASTER_SERIAL = 2,   // 桌面串行模式
    BNC_GEN_MODE_SLAVE_SERIAL = 3,    // 暗室串行模式
} BNC_GEN_Mode;

typedef struct
{
    u8 in1_polarity;  /*0:高电平有效（默认参数）；      1：低电平有效    */
    u8 in2_polarity;  /*0:高电平有效（默认参数）；      1：低电平有效    */
    u8 out1_polarity; /*0:高电平有效（默认参数）；      1：低电平有效    */
    // u8   role             ;      /*0:桌面模式（主机角色）；        1：暗室模式（从机模式）      */
    // u8   serial_mode      ;      /*0:并行发送模式（默认参数）；    1：串行发送模式  */
    BNC_GEN_Mode mode;
    u32 num;
} BNC_GEN_Config;

#define BNC_GEN_EVENT_TRIGER_VNA 1 // BIT1
#define BNC_GEN_EVENT_TRIGER_TR 2 // BIT2    // 触发TR信号

#define DEFAULT_BNC_GEN_CONFIG()        \
{                                       \
    .in1_polarity = 0,                  \
    .in2_polarity = 0,                  \
    .out1_polarity = 0,                 \
    .mode = BNC_GEN_MODE_MASTER_SERIAL, \
    .num = 0                          \
}

void bnc_gen_dev_init(void);
void bnc_gen_dev_bind_irq_callback(void (*callback)(void *user_data, u32 event), void *user_data);
int bnc_gen_dev_config(BNC_GEN_Config *config);
void bnc_gen_dev_get_config(BNC_GEN_Config *config);
void bnc_gen_start(void);
void bnc_gen_enable(void);
void bnc_gen_disable(void);
void bnc_gen_in1_enable(void);
void bnc_gen_in1_disable(void);
void bnc_gen_clr_cnt(void);
void bnc_config_num(u32 num);
u32 bnc_gen_get_out1_cnt(void);
u32 bnc_gen_get_in1_cnt(void);


#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */
/** @} */
