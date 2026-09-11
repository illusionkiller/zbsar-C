#include "platform.h"
#include "xil_io.h"
#include "bnc_gen.h"

extern XIntc XIntcInstance;
#define xInterruptController XIntcInstance
extern XIntc XIntcInstance1;
#define xInterruptController1 XIntcInstance1
/****************** registers define ********************/
#define BNC_GEN_CTRL_REG_OFFSET 0X0000
#define BNC_GEN_MODE_REG_OFFSET 0X0004
#define BNC_GEN_START_REG_OFFSET 0X0008

// registers bits
#define CTRL_REG_BNC_IN1_POLARITY  0  // bit0
#define CTRL_REG_BNC_IN2_POLARITY  1  // bit1
#define CTRL_REG_BNC_OUT1_POLARITY 2 // bit2
#define CTRL_REG_BNC_IN1_EN        3   //bit3

#define MODE_REG_ROLE_MODE 0 // bit0   0:桌面模式（主机模式） 1：暗室模式（从机模式）
#define MODE_REG_SERIAL_MODE 1 // bit1 0:串行模式 1：并行模式
#define MODE_REG_FILE_MODE              2   //bit2

typedef struct
{
    volatile u32 CTRL;          /*address offset:0x0000  (w/r)   */
    volatile u32 MODE;          /*address offset:0x0004  (w/r)   */
    volatile u32 START;         /*address offset:0x0008  (w/r)   */
    volatile u32 CLR_CNT;       /*address offset:0x000C  (w/r)   */
    volatile u32 BNC_EN;        /*address offset:0x0010  (w/r)   */
    volatile u32 OUT1_CNT;      /*address offset:0x0014  (r  )   */
    volatile u32 INT1_CNT;      /*address offset:0x0018  (r  )   */
    volatile u32 NUM;
} BNC_GEN_TypeDef;

typedef struct
{
    int irq;
    int irq1;
    uint32_t reg_base_addr;
    int opened;
    BNC_GEN_Config config;
    void (*callback)(void *user_data, u32 event);
    void *user_data;
} bnc_gen_dev_t;

static bnc_gen_dev_t bnc_gen_dev =
{
	.irq = XPAR_AXI_INTC_0_PULSE_GEN_0_SHIWANG_IRQ_ALL_INTR,
	.irq1 = XPAR_AXI_INTC_0_BNC_GEN_0_ALL_CH_IRQ_INTR,
	.reg_base_addr = XPAR_BNC_GEN_0_BASEADDR,
};

static void bnc_gen_dev_irq_handler(void *para)
{
    bnc_gen_dev_t *dev = (bnc_gen_dev_t *)para;
    //	BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)dev->reg_base_addr;
    u32 event = BNC_GEN_EVENT_TRIGER_VNA;
    if (dev->callback)
    {
        dev->callback(dev->user_data, event);
    }
}

static void bnc_gen_dev_irq1_handler(void *para)
{
    bnc_gen_dev_t *dev = (bnc_gen_dev_t *)para;
    //	BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)dev->reg_base_addr;
    u32 event = BNC_GEN_EVENT_TRIGER_TR;
    if (dev->callback)
    {
        dev->callback(dev->user_data, event);
    }
}

static int BncGenSetupIntrSystem(bnc_gen_dev_t *dev)
{
    int Status = XIntc_Connect(&xInterruptController, dev->irq,
                               (XInterruptHandler)bnc_gen_dev_irq_handler,
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
    Status = XIntc_Connect(&xInterruptController, dev->irq1,
                           (XInterruptHandler)bnc_gen_dev_irq1_handler,
                           (void *)dev);
    if (Status != XST_SUCCESS)
    {
        return -1;
    }
    return 0;
}

int bnc_gen_dev_config(BNC_GEN_Config *config)
{
    if (config == NULL)
    {
        return -1;
    }
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    BNC_GEN->CTRL = config->out1_polarity << CTRL_REG_BNC_OUT1_POLARITY |
                        config->in2_polarity << CTRL_REG_BNC_IN2_POLARITY |
                        config->in1_polarity << CTRL_REG_BNC_IN1_POLARITY;
    BNC_GEN->MODE = config->mode;
    BNC_GEN->NUM = config->num;
    bnc_gen_dev.config = *config;
    return 0;
}

void bnc_gen_dev_get_config(BNC_GEN_Config *config)
{
    if (config == NULL)
    {
        return;
    }
    bnc_gen_dev_t *dev = &bnc_gen_dev;
    *config = dev->config;
}

void bnc_gen_dev_bind_irq_callback(void (*callback)(void *user_data, u32 event), void *user_data)
{
    if (callback)
    {
        bnc_gen_dev.callback = callback;
        bnc_gen_dev.user_data = user_data;
        XIntc_Enable(&xInterruptController, bnc_gen_dev.irq);
         XIntc_Enable(&xInterruptController, bnc_gen_dev.irq1);
    }
    else
    {
         XIntc_Disable(&xInterruptController, bnc_gen_dev.irq1);
        XIntc_Disable(&xInterruptController, bnc_gen_dev.irq);
        bnc_gen_dev.callback = NULL;
        bnc_gen_dev.user_data = NULL;
    }
}

void bnc_gen_start(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    BNC_GEN->START = 1;
    BNC_GEN->START = 0;
}

void bnc_gen_enable(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    BNC_GEN->BNC_EN = 1;
}

void bnc_gen_disable(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    BNC_GEN->BNC_EN = 0;
}

void bnc_gen_clr_cnt(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    BNC_GEN->CLR_CNT = 1;
}

u32 bnc_gen_get_out1_cnt(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    return BNC_GEN->OUT1_CNT; // out1
}

u32 bnc_gen_get_in1_cnt(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    return BNC_GEN->INT1_CNT; // in1
}

void bnc_gen_in1_enable(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    SET1_BIT(BNC_GEN->CTRL, CTRL_REG_BNC_IN1_EN);
}

void bnc_gen_in1_disable(void)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    SET0_BIT(BNC_GEN->CTRL, CTRL_REG_BNC_IN1_EN);
}

void bnc_gen_dev_init(void)
{
    BNC_GEN_Config config = DEFAULT_BNC_GEN_CONFIG();
    BncGenSetupIntrSystem(&bnc_gen_dev);
    bnc_gen_dev_config(&config);
    bnc_gen_clr_cnt();
    printf("bnc_gen_dev init success.\n");

}

void bnc_config_num(u32 num)
{
    BNC_GEN_TypeDef *BNC_GEN = (BNC_GEN_TypeDef *)bnc_gen_dev.reg_base_addr;
    BNC_GEN->NUM = num;
}



