#include "platform.h"
#include "xil_io.h"
#include "spi_pl_slave.h"

extern  XIntc XIntcInstance;
#define xInterruptController XIntcInstance

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

typedef struct
{
    volatile u32 CR;        /*address offset:0x0000        //control register(w & r)                 */
    volatile u32 RSR;       /*address offset:0x0004        //rate set register(w & r)                */
    volatile u32 DLR;       /*address offset:0x0008        //data length register(w & r)             */
    volatile u32 CSR;       /*address offset:0x000c        //chip select register(w & r)             */
    volatile u32 STWR;      /*address offset:0x0010        //start work register(w & r)              */
    volatile u32 SR;        /*address offset:0x0014        //status register(r)                      */
    volatile u32 MRR;       /*address offset:0x0018        //spi module reset register(w & r)        */
    volatile u32 CFR;       /*address offset:0x001c        //clear fifo register(w & r)              */
    volatile u32 RFOR;      /*address offset:0x0020        //receiver fifo occupancy register(r)     */
    volatile u32 TFOR;      /*address offset:0x0024        //transmit fifo occupancy register(r)     */
    volatile u32 DAT0;      /*address offset:0x0028        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT1;      /*address offset:0x002c        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT2;      /*address offset:0x0030        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT3;      /*address offset:0x0034        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 DAT4;      /*address offset:0x0038        //receiver fifo(r), and transmit fifo(w)  */
    volatile u32 RES[11];   /*address offset:0x003c~0x0064                                           */
    volatile u32 MODE;      /*address offset:0x0068        //only writer                             */
    volatile u32 CTRL;      /*address offset:0x006c        //only writer                             */
    volatile u32 DELAY;     /*address offset:0x0070        //only writer                             */
    volatile u32 DUTY;      /*address offset:0x0074        //only writer                             */
    volatile u32 PERIOD;    /*address offset:0x0078        //only writer                             */
    volatile u32 SAFE_TIME; /*address offset:0x007c        //only writer                             */
    volatile u32 PULSE_NUM; /*address offset:0x0080        //only writer                             */
} SPI_Handle_Def;

static void spi_pl_slave_bus_irq_handler(void *para)
{
    spi_pl_slave_bus_t *bus = (spi_pl_slave_bus_t *)para;
//    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    u32 event = 0;
    event = SPI_PL_SLAVE_BUS_EVENT_RECEIVE_DONE;
    if (bus->callback)
    {
        bus->callback(bus->user_data, event);
    }
}

void spi_pl_slave_bus_enable(spi_pl_slave_bus_t *bus, bool enable)
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

void spi_pl_slave_bus_reset_fifo(spi_pl_slave_bus_t *bus)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    for(int i = 0;i < 10;i++)
    {
        SET1_BIT(SPI->CFR, SPI_CFR_RFR);
        SET1_BIT(SPI->CFR, SPI_CFR_TFR);
    }

    SET0_BIT(SPI->CFR, SPI_CFR_RFR);
    SET0_BIT(SPI->CFR, SPI_CFR_TFR);
}

void spi_pl_slave_bus_reset_rxfifo(spi_pl_slave_bus_t *bus)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    for(int i = 0;i < 10;i++)
    {
        SET1_BIT(SPI->CFR, SPI_CFR_RFR);
    }
    SET0_BIT(SPI->CFR, SPI_CFR_RFR);
}


size_t spi_pl_slave_bus_recv(spi_pl_slave_bus_t *bus, uint8_t *RecvBuf[5], u32 ByteCount)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    if (ByteCount < SPI_PL_SLAVE_FIFO_DEPTH)  //读空fifo
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
        if (RecvBuf[4])
            *(RecvBuf[4] + read_count) = SPI->DAT4;
        read_count++;
        if (read_count >= ByteCount)
            break;
    }
    return read_count;
}

size_t spi_pl_slave_bus_recv_data4(spi_pl_slave_bus_t *bus, uint8_t *RecvBuf, u32 ByteCount)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
//    if (ByteCount < SPI_PL_SLAVE_FIFO_DEPTH)  //读空fifo
//    {
//        return -1;
//    }
    u32 read_count = 0;
//    while (GET_BIT(SPI->SR, SPI_SR_REF) == 0) // not empty
    while(1)
    {
        if (RecvBuf)
            *(RecvBuf + read_count) = SPI->DAT4;
        read_count++;
        if (read_count >= ByteCount)
            break;
    }
    return read_count;
}


static int SpiPlSetupIntrSystem(spi_pl_slave_bus_t *bus)
{
    configASSERT(bus);
    int Status;

    Status = XIntc_Connect(&xInterruptController, bus->irq,
                            (XInterruptHandler)spi_pl_slave_bus_irq_handler,
                            (void *)bus);
    if (Status != XST_SUCCESS)
    {
        return -1;
    }
	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the SPI can cause interrupts through the interrupt controller.
	 */


    return 0;
}

void spi_pl_slave_bus_bind_irq_callback(spi_pl_slave_bus_t *bus, void (*callback)(void *user_data, u32 event),void *user_data)
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

static spi_pl_slave_bus_t spi_pl_slave_bus[] =  {
    {
        .id = 0,
        .irq = XPAR_AXI_INTC_0_SPI_SLAVE_1_DONE_INTR,
        .base_addr = XPAR_SPI_SLAVE_1_BASEADDR,
    },
    {
        .id = 1,
        .irq = XPAR_AXI_INTC_0_SPI_SLAVE_2_DONE_INTR,
        .base_addr = XPAR_SPI_SLAVE_2_BASEADDR,
    },
    {
        .id = 2,
        .irq = XPAR_AXI_INTC_0_SPI_SLAVE_3_DONE_INTR,
        .base_addr = XPAR_SPI_SLAVE_3_BASEADDR,
    },
    {
        .id = 3,
        .irq = XPAR_AXI_INTC_0_SPI_SLAVE_4_DONE_INTR,
        .base_addr = XPAR_SPI_SLAVE_4_BASEADDR,
    },
    {
        .id = 4,
        .irq = XPAR_AXI_INTC_0_SPI_SLAVE_5_DONE_INTR,
        .base_addr = XPAR_SPI_SLAVE_5_BASEADDR,
    },
};

spi_pl_slave_bus_t *spi_pl_slave_bus_get_handle(int id)
{
    for (int i = 0; i < sizeof(spi_pl_slave_bus) / sizeof(spi_pl_slave_bus[0]); i++)
    {
        if(spi_pl_slave_bus[i].id == id)
        {
            return &spi_pl_slave_bus[i];
        }
    }
    return NULL;
}

size_t spi_pl_slave_bus_write(spi_pl_slave_bus_t *bus, uint8_t *SendBuf[5], u32 ByteCount)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    if (ByteCount > SPI_PL_SLAVE_FIFO_DEPTH)
    {
        return -1;
    }
    if (GET_BIT(SPI->SR, SPI_SR_TEF) == 0) // not empty
    {
        return -1;
    }

    if (ByteCount == 0)
        return 0;
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
        if (SendBuf[4])
            SPI->DAT4 = *(SendBuf[4] + i);
    }

    return ByteCount;
}

void spi_pl_slave_bus_start_transfer(spi_pl_slave_bus_t *bus)
{
    SPI_Handle_Def *SPI = (SPI_Handle_Def *)bus->base_addr;
    SET1_BIT(SPI->STWR, 0);
    SET0_BIT(SPI->STWR, 0);
}

void spi_pl_slave_bus_init(void)
{
    for (int i = 0; i < sizeof(spi_pl_slave_bus) / sizeof(spi_pl_slave_bus[0]); i++)
    {
        spi_pl_slave_bus_enable(&spi_pl_slave_bus[i], false);
        spi_pl_slave_bus_reset_fifo(&spi_pl_slave_bus[i]);
        SpiPlSetupIntrSystem(&spi_pl_slave_bus[i]);
        printf("spi_pl_slave_bus[%d] init done\n", i);
    }
}


