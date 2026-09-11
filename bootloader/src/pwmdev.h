#ifndef __PWMDEV_H_
#define __PWMDEV_H_
#include "xil_types.h"
/****************** Include Files ********************/
#define PWM_CHANNEL_MAX_CLOCK 999999 // 1000ms
#define PWM_CHANNEL_START_NUM 0
#define PWM_CHANNEL_END_NUM 39
#define PWM_MAX_CHANNEL_NUM (PWM_CHANNEL_END_NUM - PWM_CHANNEL_START_NUM)
enum pwm_dev_ioctl_cmd
{
	PWM_DEV_IOCTL_SET_PERIOD = 0,
    PWM_DEV_IOCTL_GET_PERIOD,
	PWM_DEV_IOCTL_SET_DUTY,
    PWM_DEV_IOCTL_GET_DUTY,
    PWM_DEV_IOCTL_ONE_PULSE_MODE,
    PWM_DEV_IOCTL_CONTINUOUS_WAVE_MODE,
    PWM_DEV_IOCTL_ENABLE,
    PWM_DEV_IOCTL_DISABLE,
    PWM_DEV_IOCTL_END
};

typedef struct
{
    uint32_t channel;
    uint32_t value;
} pwm_dev_data_t;

void pwm_dev_init(void);
int pwm_dev_open(char *name ,int openflag);
int pwm_dev_close(int dev_id);
int pwm_dev_ioctl(int dev_id, int cmd, void *arg);
void print_pwm_dev_info();
void register_pwm_commands(void);

#endif // PWMDEV_H
