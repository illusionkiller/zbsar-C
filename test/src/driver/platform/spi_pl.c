#include "platform.h"
#include "xil_io.h"
#include "spi_pl.h"

extern XIntc XIntcInstance;
#define xInterruptController XIntcInstance

//#define SPI_PL_CLK_FREQ_HZ (100000000 / 2)
#define SPI_PL_CLK_FREQ_HZ (100000000)
#define SPI_PL_MAX_CHIP_SEL (31)

/****************** registers define ********************/
#define SPI_CR_OFFSET 0x0000   // control register(w & r)
#define SPI_RSR_OFFSET 0x0004  // rate set register(w)
#define SPI_DLR_OFFSET 0x0008  // data length register(w)
#define SPI_CSR_OFFSET 0x000c  // chip select register(w)
#define SPI_STWR_OFFSET 0x0010 // start work register(w)
#define SPI_SR_OFFSET 0x0014   // status register(r)
#define SPI_MRR_OFFSET 0x0018  // spi module reset register(w)
#define SPI_CFR_OFFSET 0x001c  // clear fifo register(w)
#define SPI_RFOR_OFFSET 0x0020 // receiver fifo occupancy register(r)
#define SPI_TFOR_OFFSET 0x0024 // transmit fifo occupancy register(r)

// FIFO ADDRESS
#define SPI_RF_OFFSET 0x0028 // receiver fifo(r)
#define SPI_TF_OFFSET 0x0028 // transmit fifo(w)

#define SPI_DAT0_OFFSET 0x0028 // DAT0 line fifo
#define SPI_DAT1_OFFSET 0x002c // DAT1 line fifo
#define SPI_DAT2_OFFSET 0x0030 // DAT2 line fifo
#define SPI_DAT3_OFFSET 0x0034 // DAT3 line fifo

// pwm ctrl register(w)
#define MODE_OFFSET 0x0068
#define CTRL_OFFSET 0x006c
#define DELAY_OFFSET 0x0070
#define DUTY_OFFSET 0x0074
#define PERIOD_OFFSET 0x0078
#define SAFE_TIME_OFFSET 0x007c
#define PULSE_NUM_OFFSET 0x0080

/****************** registers bits define ********************/
#define SPI_CR_RIE 6  // receiver interrupt enable
#define SPI_CR_TIE 5  // transmit interrupt enable
#define SPI_CR_MSS 4  // master or slave select
#define SPI_CR_LMF 2  // lsb first or msb first(0->msb first,1->lsb first)
#define SPI_CR_SWAI 1 // clk status under stop state(0->normal,1->stop)
#define SPI_CR_RFS 0  // sample on clk's raise edge or fall edge select(0->raise,1->fall)

#define SPI_SR_REF 1 // receiver fifo empty
#define SPI_SR_TEF 0 // transmit fifo empty

#define SPI_CFR_RFR 1 // receiver fifo reset
#define SPI_CFR_TFR 0 // transmit fifo reset

#define CTRL_POLARITY 2 // Polarity
/**************************** Type Definitions *****************************/
/**
 *
 * Write a value to a SPI register. A 32 bit write is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is written.
 *
 * @param   BaseAddress is the base address of the device.
 * @param   RegOffset is the register offset from the base to write to.
 * @param   Data is the data written to the register.
 *
 * @return  None.
 *
 * @note
 * C-style signature:
 *  void Spi_WriteReg(u32 BaseAddress, unsigned RegOffset, u32 Data)
 *
 */
#define Spi_WriteReg(BaseAddress, RegOffset, Data) \
    Xil_Out32((BaseAddress) + (RegOffset), (u32)(Data))

/**
 *
 * Read a value from a SPI register. A 32 bit read is performed.
 * If the component is implemented in a smaller width, only the least
 * significant data is read from the register. The most significant data
 * will be read as 0.
 *
 * @param   BaseAddress is the base address of the device.
 * @param   RegOffset is the register offset from the base to write to.
 *
 * @return  Data is the data from the register.
 *
 * @note
 * C-style signature:
 * 	u32 Spi_ReadReg(u32 BaseAddress, unsigned RegOffset)
 *
 */
#define Spi_ReadReg(BaseAddress, RegOffset) \
    Xil_In32((BaseAddress) + (RegOffset))

typedef struct
{
    volatile u32 CR;        /*address offset:0x0000        //control register(w & r)                 */
    volatile u32 RSR;       /*address offset:0x0004        //rate set register(w)                    */
    volatile u32 DLR;       /*address offset:0x0008        //data length register(w)                 */
    volatile u32 CSR;       /*address offset:0x000c        //chip select register(w)                 */
    volatile u32 STWR;      /*address offset:0x0010        //start work register(w)                  */
    volatile u32 SR;        /*address offset:0x0014        //status register(r)                      */
    volatile u32 MRR;       /*address offset:0x0018        //spi module reset register(w)            */
    volatile u32 CFR;       /*address offset:0x001c        //clear fifo register(w)                  */
    volatile u32 RFOR;      /*address offset:0x0020        //receiver fifo occupancy register(r)     */
    volatile u32 TFOR;      /*address offset:0x0024        //transmit fifo occupancy register(r)     */
                            //    volatile u32  DATA[4]       ;      /*address offset:0x0028~0x0034 //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT0;      /*address offset:0x0028        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT1;      /*address offset:0x002c        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT2;      /*address offset:0x0030        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT3;      /*address offset:0x0034        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 RES[12];   /*address offset:0x0038~0x0064                                           */
    volatile u32 MODE;      /*address offset:0x0068                                                  */
    volatile u32 CTRL;      /*address offset:0x006c                                                  */
    volatile u32 DELAY;     /*address offset:0x0070                                                  */
    volatile u32 DUTY;      /*address offset:0x0074                                                  */
    volatile u32 PERIOD;    /*address offset:0x0078                                                  */
    volatile u32 SAFE_TIME; /*address offset:0x007c                                                  */
    volatile u32 PULSE_NUM; /*address offset:0x0080                                                  */
} SPI_Handle_Def;



static void spi_pl_bus_irq_handler(void *para)
{
    spi_pl_bus_t *bus = (spi_pl_bus_t *)para;
//    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    u32 event = 0;
//    if (GET_BIT(SPI->SR, SPI_SR_TEF)) // transfer done
//    {
//        SET1_BIT(event, SPI_PL_BUS_EVENT_TRANSFER_DONE);
//    }
//    if (!GET_BIT(SPI->SR, SPI_SR_REF)) // receiver fifo full
//    {
//        SET1_BIT(event, SPI_PL_BUS_EVENT_RECEV_DONE);
//    }
//    event = SPI->SR;
    event = SPI_PL_BUS_EVENT_TRANSFER_DONE;
    if (bus->callback)
    {
        bus->callback(bus->user_data, event);
    }
}

void spi_pl_bus_enable(spi_pl_bus_t *bus, bool enable)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    if (enable)
    {
        SET1_BIT(SPI->MRR, 0);
    }
    else
    {
        SET0_BIT(SPI->MRR, 0);
    }
}

void spi_pl_bus_reset_fifo(spi_pl_bus_t *bus)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    for(int i = 0;i < 10;i++)
    {
        SET1_BIT(SPI->CFR, SPI_CFR_RFR);
        SET1_BIT(SPI->CFR, SPI_CFR_TFR);
    }
    SET0_BIT(SPI->CFR, SPI_CFR_RFR);
    SET0_BIT(SPI->CFR, SPI_CFR_TFR);
    usleep(10);
}

int spi_pl_bus_config(spi_pl_bus_t *bus, spi_pl_config_t *config)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    SPI->CR = config->Wait_Clk << SPI_CR_SWAI | config->Sample_Edge_Sel << SPI_CR_RFS | config->First_Bit << SPI_CR_LMF;
    config->BaudRate = config->BaudRate == 0 ? 2000 : config->BaudRate;
    u16 rate = 0;
    if (SPI_PL_CLK_FREQ_HZ % config->BaudRate) //
    {
        rate = (u16)(SPI_PL_CLK_FREQ_HZ / config->BaudRate) + 1;  //取低频
    }
    else
    {
        rate = (u16)(SPI_PL_CLK_FREQ_HZ / config->BaudRate);
    }
    SPI->RSR = rate > 4 ? rate : 10;
    // reset rx and tx fifo
    SPI->CTRL = config->Polarity << CTRL_POLARITY;
    SPI->DELAY = config->Delay;
    SPI->DUTY = config->Pulse_Width;
    memcpy(&bus->config, config, sizeof(spi_pl_config_t));
    return 0;
}

void spi_pl_bus_get_config(spi_pl_bus_t *bus, spi_pl_config_t *config)
{
    memcpy(config, &bus->config, sizeof(spi_pl_config_t));
}



void spi_pl_bus_set_transmit_bits_len(spi_pl_bus_t *bus, u32 bits_len)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    SPI->DLR = bits_len;
}
int spi_pl_bus_write(spi_pl_bus_t *bus, u8 *SendBuf[4], u32 ByteCount)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    if(ByteCount > SPI_PL_FIFO_DEPTH)
    {
        return -1;
    }
    if (GET_BIT(SPI->SR, SPI_SR_TEF) == 0) // not empty
    {
        return -1;
    }

    if(ByteCount == 0) return 0;
    for (u32 i = 0; i < ByteCount; i++)
    {
        if (SendBuf[0])
            SPI->DAT0 = *(SendBuf[0] + i);
        if (SendBuf[1])
            SPI->DAT1 = *(SendBuf[1] + i);
        if (SendBuf[2])
            SPI->DAT2 = *(SendBuf[2] + i);
        if (SendBuf[3])
            SPI->DAT3 = *(SendBuf[3] + i);
    }

    return ByteCount;
}

void spi_pl_bus_start_transfer(spi_pl_bus_t *bus)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    SET1_BIT(SPI->STWR, 0);
    SET0_BIT(SPI->STWR, 0);
}

int spi_pl_bus_recv(spi_pl_bus_t *bus, u8 *RecvBuf[4], u32 ByteCount)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    if (ByteCount < SPI_PL_RECV_FIFO_DEPTH)  //读空fifo
    {
        return -1;
    }
    u32 read_count = 0;
    while (GET_BIT(SPI->SR, SPI_SR_REF) == 0) // not empty
    {
        if (RecvBuf[0])
            *(RecvBuf[0] + read_count) = SPI->DAT0;
        if (RecvBuf[1])
            *(RecvBuf[1] + read_count) = SPI->DAT1;
        if (RecvBuf[2])
            *(RecvBuf[2] + read_count) = SPI->DAT2;
        if (RecvBuf[3])
            *(RecvBuf[3] + read_count) = SPI->DAT3;

        read_count++;
        if(read_count >= ByteCount) break;

    }
    return read_count;
}

void spi_pl_bus_select(spi_pl_bus_t *bus, u8 Chip_Sel)
{
    if(Chip_Sel >= SPI_PL_MAX_CHIP_SEL)
    {
        return;
    }
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    SET1_BIT(SPI->CSR, Chip_Sel);
}

static int SpiPlSetupIntrSystem(spi_pl_bus_t *bus)
{
    configASSERT(bus);

	int Status = XIntc_Connect(&xInterruptController,bus->irq,
				(XInterruptHandler)spi_pl_bus_irq_handler,
				(void *)bus);
	if(Status != XST_SUCCESS) {
		return -1;
	}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the SPI can cause interrupts through the interrupt controller.
	 */
	// Status = XIntc_Start(&xInterruptController, XIN_REAL_MODE);
	// if(Status != XST_SUCCESS) {
	// 	return -1;
	// }

    return 0;
}

void spi_pl_bus_bind_irq_callback(spi_pl_bus_t *bus, void (*callback)(void *user_data, u32 event),void *user_data)
{
    if(callback)
    {
        bus->callback = callback;
        bus->user_data = user_data;
    	XIntc_Enable(&xInterruptController, bus->irq);
    }
    else
    {
    	XIntc_Disable(&xInterruptController, bus->irq);
        bus->callback = NULL;
        bus->user_data = NULL;
    }
}

static spi_pl_bus_t spi_pl_bus[] = {
    {
        .id = 0,
        .irq = XPAR_AXI_INTC_0_IRQ_CH1_RES_INTR,
        .base_addr = XPAR_SPI_MASTER_1_BASEADDR, 
        .config = DEFAULT_SPI_PL_BUS_CONFIG(),
    },
    {
        .id = 1,
        .irq = XPAR_AXI_INTC_0_IRQ_CH2_RES_INTR,
        .base_addr = XPAR_SPI_MASTER_2_BASEADDR,
        .config = DEFAULT_SPI_PL_BUS_CONFIG(),
    },
    {
        .id = 2,
        .irq = XPAR_AXI_INTC_0_IRQ_CH3_RES_INTR,
        .base_addr = XPAR_SPI_MASTER_3_BASEADDR, 
        .config = DEFAULT_SPI_PL_BUS_CONFIG(),
    },
    {
        .id = 3,
        .irq = XPAR_AXI_INTC_0_IRQ_CH4_RES_INTR,
        .base_addr = XPAR_SPI_MASTER_4_BASEADDR, 
        .config = DEFAULT_SPI_PL_BUS_CONFIG(),
    },
    {
        .id = 4,
        .irq = XPAR_AXI_INTC_0_IRQ_CH5_RES_INTR,
        .base_addr = XPAR_SPI_MASTER_5_BASEADDR, // TTL
        .config = DEFAULT_SPI_PL_BUS_CONFIG(),
    }
};

spi_pl_bus_t *spi_pl_bus_get_handle(int id)
{
    for (int i = 0; i < sizeof(spi_pl_bus) / sizeof(spi_pl_bus[0]); i++)
    {
        if(spi_pl_bus[i].id == id)
        {
            return &spi_pl_bus[i];
        }
    }
    return NULL;
}

void spi_pl_bus_init(void)
{
    for (int i = 0; i < sizeof(spi_pl_bus) / sizeof(spi_pl_bus[0]); i++)
    {
        spi_pl_bus_enable(&spi_pl_bus[i], false);
        spi_pl_bus_reset_fifo(&spi_pl_bus[i]);
        spi_pl_bus_config(&spi_pl_bus[i], &spi_pl_bus[i].config);
        SpiPlSetupIntrSystem(&spi_pl_bus[i]);
        spi_pl_bus_select(&spi_pl_bus[i], 1);
        printf("spi_pl_bus[%d] init done\n", i);
    }
}


