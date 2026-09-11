#ifndef __SPI_PL_H_ /* prevent circular inclusions */
#define __SPI_PL_H_ /* by using protection macros */
#ifdef __cplusplus
extern "C"
{
#endif

#include "xil_types.h"
#include "stdbool.h"
typedef struct
{
    u8 Wait_Clk;        /*clk status under stop state(0->normal,1->stop)  */
    u8 Sample_Edge_Sel; /*sample on clk's raise edge or fall edge select(0->raise,1->fall)  */
    u8 First_Bit;       /*lsb first or msb first(0->msb first,1->lsb first)  */
    u32 BaudRate;           /*SCLK rate=ClockFreq/Rate/2  */
                        //    u32  Mode                   ;      /*PWM ctrl:  */
    u8 Polarity;        /*PWM ctrl:  */
    u32 Delay;          /*PWM ctrl:  */
    u32 Pulse_Width;    /*PWM ctrl:1~10(us)  */
    //    u32  Period                 ;      /*PWM ctrl:  */
    //    u32  Safe_Time              ;      /*PWM ctrl:  */
    //    u32  Pulse_Num              ;      /*PWM ctrl:  */
    bool self_loop;
} spi_pl_config_t;

typedef struct
{
    int id;
    int irq;
    u32 base_addr;
    spi_pl_config_t config;
    void (*callback)(void *user_data, u32 event);
    void *user_data;
} spi_pl_bus_t;
#define SPI_PL_FIFO_DEPTH (30 * 533)
#define SPI_PL_RECV_FIFO_DEPTH (1024)
#define SPI_PL_BUS_EVENT_TRANSFER_DONE 0   //BIT0
//#define SPI_PL_BUS_EVENT_RECEV_DONE 1   //BIT1

#define DEFAULT_SPI_PL_BUS_CONFIG() { \
    .Wait_Clk = 0,           \
    .Sample_Edge_Sel = 0,    \
    .First_Bit = 0,          \
    .BaudRate = 4000000,               \
    .Polarity = 1,           \
    .Delay = 10,              \
    .Pulse_Width = 1,        \
    .self_loop = false       \
}

/************************** Function Prototypes ****************************/
void spi_pl_bus_select(spi_pl_bus_t *bus, u8 Chip_Sel);
void spi_pl_bus_bind_irq_callback(spi_pl_bus_t *bus, void (*callback)(void *user_data, u32 event),void *user_data);
void spi_pl_bus_enable(spi_pl_bus_t *bus, bool enable);
void spi_pl_bus_reset_fifo(spi_pl_bus_t *bus);
int spi_pl_bus_config(spi_pl_bus_t *bus, spi_pl_config_t *config);
void spi_pl_bus_get_config(spi_pl_bus_t *bus, spi_pl_config_t *config);
int spi_pl_bus_recv(spi_pl_bus_t *bus, u8 *RecvBuf[4],u32 ByteCount);
int spi_pl_bus_write(spi_pl_bus_t *bus, u8 *SendBuf[4], u32 ByteCount);
void spi_pl_bus_start_transfer(spi_pl_bus_t *bus);
void spi_pl_bus_set_transmit_bits_len(spi_pl_bus_t *bus, u32 bits_len);
spi_pl_bus_t *spi_pl_bus_get_handle(int id);
void spi_pl_bus_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SPI_PL_H__ */
/** @} */
