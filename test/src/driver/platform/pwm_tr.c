#include "platform.h"
#include "xil_io.h"
#include "pwm_tr.h"
/************************** Function Definitions ***************************/
#define CHANNAL_OFFSET (1024)
#define CTRL_POLARITY 2 // Polarity

extern XIntc XIntcInstance;
#define xInterruptController XIntcInstance
typedef struct
{
	volatile u32 MODE;		/*address offset(w & r):0x0000  */
	volatile u32 CTRL;		/*address offset(w & r):0x0004  */
	volatile u32 DELAY;		/*address offset(w & r):0x0008  */
	volatile u32 DUTY;		/*address offset(w & r):0x000c  */
	volatile u32 PERIOD;	/*address offset(w & r):0x0010  */
	volatile u32 SAFE_TIME; /*address offset(w & r):0x0014  */
	volatile u32 PULSE_NUM; /*address offset(w & r):0x0018  */
	volatile u32 TRIG_OUT;	/*address offset(w & r):0x001c  */
    volatile u32 TR_EN      ;      /*address offset(w & r):0x0020  */
	volatile u32 IRQ_FLAG;	/*address offset(r    ):0x0020  */
} PULSE_TypeDef;
typedef struct
{
	int dev_id;
	int irq;
	uint32_t reg_base_addr;
	pulse_Config config;
	void (*callback)(void *user_data, u32 event);
	void *user_data;
} pwm_tr_dev_t;

static pwm_tr_dev_t pwm_tr_dev[PWM_TR_MAX_CHANNEL_NUM] = {
	{
		.dev_id = 0,
		.irq = XPAR_AXI_INTC_0_PULSE_GEN_0_IRQ_INTR,
		.reg_base_addr = XPAR_PULSE_GEN_0_BASEADDR + CHANNAL_OFFSET * 0,
	},
	{
		.dev_id = 1,
		.irq = XPAR_AXI_INTC_0_PULSE_GEN_0_IRQ_LOW_PRIORITY_INTR,
		.reg_base_addr = XPAR_PULSE_GEN_0_BASEADDR + CHANNAL_OFFSET * 1,
	},
	{
		.dev_id = 2,
		.irq = XPAR_AXI_INTC_0_PULSE_GEN_0_IRQ_2_INTR,
		.reg_base_addr = XPAR_PULSE_GEN_0_BASEADDR + CHANNAL_OFFSET * 2,
	},
	{
		.dev_id = 3,
		.irq = XPAR_AXI_INTC_0_PULSE_GEN_0_IRQ_3_INTR,
		.reg_base_addr = XPAR_PULSE_GEN_0_BASEADDR + CHANNAL_OFFSET * 3,
	},
	{
		.dev_id = 4,
		.irq = XPAR_AXI_INTC_0_PULSE_GEN_0_IRQ_4_INTR,
		.reg_base_addr = XPAR_PULSE_GEN_0_BASEADDR + CHANNAL_OFFSET * 4,
	},
//	{
//		.dev_id = 5,
//		.irq = XPAR_AXI_INTC_0_PULSE_GEN_0_IRQ_5_INTR,
//		.reg_base_addr = XPAR_PULSE_GEN_0_BASEADDR + CHANNAL_OFFSET * 5,
//	},
};

static void pwm_tr_dev_irq_handler(void *para)
{
	pwm_tr_dev_t *dev = (pwm_tr_dev_t *)para;
//	PULSE_TypeDef *PULSE_CHANNAL = (PULSE_TypeDef *)dev->reg_base_addr;
	u32 event = PWM_TR_EVENT_TRANSFER_DONE;
	if (dev->callback)
	{
		dev->callback(dev->user_data, event);
	}
}

int pwm_tr_dev_open(int dev_id, int openflag)
{
	if (dev_id >= PWM_TR_MAX_CHANNEL_NUM)
	{
		return -1;
	}
	return 0;
}

void pwm_tr_dev_close(int dev_id)
{
	if (dev_id >= PWM_TR_MAX_CHANNEL_NUM)
	{
		return;
	}
	return ;
}

int pwm_tr_dev_config(int dev_id, pulse_Config *config)
{
	if (dev_id >= PWM_TR_MAX_CHANNEL_NUM)
	{
		return -1;
	}
	PULSE_TypeDef *PULSE_CHANNAL = (PULSE_TypeDef *)pwm_tr_dev[dev_id].reg_base_addr;
	PULSE_CHANNAL->MODE = config->Mode;	
	PULSE_CHANNAL->CTRL = 1 << CTRL_POLARITY; // 高有效
	PULSE_CHANNAL->DELAY = config->Delay;
	PULSE_CHANNAL->DUTY = config->Pulse_Width;
	PULSE_CHANNAL->PERIOD = config->Period;
	PULSE_CHANNAL->SAFE_TIME = config->Safe_Time;
	PULSE_CHANNAL->PULSE_NUM = config->Pulse_Num;
	PULSE_CHANNAL->TRIG_OUT = config->Trig_Delay;
	PULSE_CHANNAL->TR_EN = (config->TR_V_EN << 1) | (config->TR_H_EN << 0);
	memcpy(&pwm_tr_dev[dev_id].config, config, sizeof(pulse_Config));
	return 0;
}

void pwm_tr_dev_get_config(int dev_id, pulse_Config *config)
{
	if (dev_id >= PWM_TR_MAX_CHANNEL_NUM)
	{
		return;
	}
	memcpy(config, &pwm_tr_dev[dev_id].config, sizeof(pulse_Config));	
}

static int PwmTrSetupIntrSystem(pwm_tr_dev_t *dev)
{
	configASSERT(dev);

	int Status = XIntc_Connect(&xInterruptController, dev->irq,
							   (XInterruptHandler)pwm_tr_dev_irq_handler,
							   (void *)dev);
	if (Status != XST_SUCCESS)
	{
		return -1;
	}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the SPI can cause interrupts through the interrupt controller.
	 */
	// Status = XIntc_Start(&xInterruptController, XIN_REAL_MODE);
	// if (Status != XST_SUCCESS)
	// {
	// 	return XST_FAILURE;
	// }

	return 0;
}

void pwm_tr_dev_bind_irq_callback(int dev_id , void (*callback)(void *user_data, u32 event), void *user_data)
{
	if (dev_id >= PWM_TR_MAX_CHANNEL_NUM)
	{
		return;
	}
	if (callback)
	{
		pwm_tr_dev[dev_id].callback = callback;
		pwm_tr_dev[dev_id].user_data = user_data;
		XIntc_Enable(&xInterruptController, pwm_tr_dev[dev_id].irq);
	}
	else
	{
		XIntc_Disable(&xInterruptController, pwm_tr_dev[dev_id].irq);
		pwm_tr_dev[dev_id].callback = NULL;
		pwm_tr_dev[dev_id].user_data = NULL;
	}
}

void pwm_tr_dev_init(void)
{
	pulse_Config config = DEFAULT_PWM_TR_CONFIG();
	for (int dev_id = 0; dev_id < sizeof(pwm_tr_dev) / sizeof(pwm_tr_dev[0]); dev_id++)
	{
		PwmTrSetupIntrSystem(&pwm_tr_dev[dev_id]);
		pwm_tr_dev_config(dev_id, &config);
		printf("pwm_tr_dev[%d] init success.\n",dev_id );
	}
}
