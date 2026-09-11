#include "platform.h"
#include "xil_io.h"
#include "pwmdev.h"
/************************** Function Definitions ***************************/
#define PWM_AXI_CTRL_REG_OFFSET (0)
#define PWM_AXI_STATUS_REG_OFFSET (1024)
#define PWM_AXI_PERIOD_REG_OFFSET (2048)
#define PWM_AXI_DUTY_REG_OFFSET (3072)


									 // registers bits
#define CTRL_REG_ENABLE 0			 // bit0
#define CTRL_REG_ONE_PULSE 1		 // bit1
typedef struct
{
	char *name;
	uint32_t reg_base_addr;
} pwm_dev_t;

static pwm_dev_t pwm_dev[] = {
	{
		.name = "oc",
		.reg_base_addr = XPAR_OC_BASEADDR,
	},
	{
		.name = "oe",
		.reg_base_addr = XPAR_OE_BASEADDR,
	},
	{
		.name = "rst",
		.reg_base_addr = XPAR_RESET_BASEADDR,
	},
	{
		.name = "fan",
		.reg_base_addr = XPAR_FAN_CTRL_BASEADDR,
	},
};

int pwm_dev_open(char *name ,int openflag)
{
	int i = 0;
	for (i = 0; i < sizeof(pwm_dev) / sizeof(pwm_dev[0]); i++)
	{
		if (strcmp(name, pwm_dev[i].name) == 0)
		{
			break;
		}
	}
	if (i >= sizeof(pwm_dev) / sizeof(pwm_dev[0]))
	{
		return -1;
	}
	return i;
}

int pwm_dev_close(int dev_id)
{
	return 0;
}

int pwm_dev_ioctl(int dev_id, int cmd, void *arg)
{
	pwm_dev_data_t *pwm_data = (pwm_dev_data_t *)arg;
	uint32_t baseAddr = pwm_dev[dev_id].reg_base_addr;
	uint32_t pwmIndex = pwm_data->channel;
	if(pwmIndex > PWM_MAX_CHANNEL_NUM)
	{
		return -1;
	}
	switch (cmd)
	{
		case PWM_DEV_IOCTL_SET_PERIOD:
			Xil_Out32(baseAddr + PWM_AXI_PERIOD_REG_OFFSET + (4 * pwmIndex), pwm_data->value);
			break;
		case PWM_DEV_IOCTL_SET_DUTY:
			if(pwm_data->value > PWM_CHANNEL_MAX_CLOCK)
			{
				pwm_data->value = PWM_CHANNEL_MAX_CLOCK;
			}
			Xil_Out32(baseAddr + PWM_AXI_DUTY_REG_OFFSET + (4 * pwmIndex), pwm_data->value);
			break;
		case PWM_DEV_IOCTL_GET_PERIOD:
			pwm_data->value = Xil_In32(baseAddr + PWM_AXI_PERIOD_REG_OFFSET + (4 * pwmIndex));
			break;
		case PWM_DEV_IOCTL_GET_DUTY:
			pwm_data->value = Xil_In32(baseAddr + PWM_AXI_DUTY_REG_OFFSET + (4 * pwmIndex));
			break;
		case PWM_DEV_IOCTL_ONE_PULSE_MODE:
			{
				 uint32_t read_reg = Xil_In32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex));
				 Xil_Out32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex), (read_reg | (1 << CTRL_REG_ONE_PULSE)));
			}
			break;
		case PWM_DEV_IOCTL_CONTINUOUS_WAVE_MODE:
			{
				 uint32_t read_reg = Xil_In32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex));
				 Xil_Out32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex), (read_reg & (~(1 << CTRL_REG_ONE_PULSE))));
			}
			break;
		case PWM_DEV_IOCTL_ENABLE:
			{
				 uint32_t read_reg = Xil_In32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex));
				 Xil_Out32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex), (read_reg | (1 << CTRL_REG_ENABLE)));
			}
			break;
		case PWM_DEV_IOCTL_DISABLE:
			{
				 uint32_t read_reg = Xil_In32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex));
				 Xil_Out32(baseAddr + PWM_AXI_CTRL_REG_OFFSET + (4 * pwmIndex), (read_reg & (~(1 << CTRL_REG_ENABLE))));
			}
			break;
		default:
			break;
	}
	return 0;
}

void print_pwm_dev_info(void)
{
	printf("PWM Devices:\n");
	for (int i = 0; i < sizeof(pwm_dev) / sizeof(pwm_dev[0]); i++)
	{
		printf("[%s] reg_base_addr:0x%lx\n", pwm_dev[i].name, pwm_dev[i].reg_base_addr);
		printf("Channel range: 0-%d\n", PWM_MAX_CHANNEL_NUM);
		printf("Period/duty range: 0-%d\n", PWM_CHANNEL_MAX_CLOCK);
	}
}

void pwm_dev_init(void)
{
	pwm_dev_data_t pwm_data = {0};
	for (int dev_id = 0; dev_id < sizeof(pwm_dev) / sizeof(pwm_dev[0]); dev_id++)
	{
		for (int index = 0; index <= PWM_MAX_CHANNEL_NUM; index++)
		{
			pwm_data.channel = index;
			pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
			pwm_data.value = PWM_CHANNEL_MAX_CLOCK;
			pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_PERIOD, &pwm_data);
			pwm_data.value = 0;
			pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_DUTY, &pwm_data);
			pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ONE_PULSE_MODE, &pwm_data);
//			pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ENABLE, &pwm_data);
		}
		printf("pwm_dev[%s] init success.\n", pwm_dev[dev_id].name);
	}
}
