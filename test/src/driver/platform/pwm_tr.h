#ifndef PWM_TR_H
#define PWM_TR_H
#include "xil_types.h"
/****************** Include Files ********************/
#define PWM_TR_MAX_CHANNEL_NUM 5

typedef struct
{
    u32 Mode;        /*0:待机模式; 1:脉冲发射模式; 2:脉冲收发模式; 3:连续接收模式;  */
                     //  u8   Polarity               ;      /*  */
    u32 Delay;       /*(us)  */
    u32 Pulse_Width; /*(us)  */
    u32 Period;      /*(us)  */
    u32 Safe_Time;   /*(us)  */
    u32 Pulse_Num;   /*  */
    u32 Trig_Delay;  /*(us)  */
    u32 TR_H_EN;     /*1:RT_T/R_H使能; 0:禁止，默认使其使能  */
    u32 TR_V_EN;     /*1:RT_T/R_V使能; 0:禁止，默认使其使能  */
} pulse_Config;

#define DEFAULT_PWM_TR_CONFIG() \
{ \
    .Mode = 0, \
    .Delay = 1, \
    .Pulse_Width = 35, \
    .Period = 250, \
    .Safe_Time = 8, \
    .Pulse_Num = 1000, \
    .Trig_Delay = 500, \
	.TR_H_EN = 1, \
	.TR_V_EN = 1,\
}
#define PWM_TR_EVENT_TRANSFER_DONE 1 // BIT1

void pwm_tr_dev_init(void);
int pwm_tr_dev_open(int dev_id, int openflag);
void pwm_tr_dev_close(int dev_id);
void pwm_tr_dev_bind_irq_callback(int dev_id, void (*callback)(void *user_data, u32 event), void *user_data);
int pwm_tr_dev_config(int dev_id, pulse_Config *config);
void pwm_tr_dev_get_config(int dev_id, pulse_Config *config);


#endif // PWM_TR_DEV_H
