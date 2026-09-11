#ifndef __SPI_PL_SLAVE_H__ /* prevent circular inclusions */
#define __SPI_PL_SLAVE_H__ /* by using protection macros */

#ifdef __cplusplus
extern "C"
{
#endif

#include "xil_types.h"
#include "stdbool.h"

typedef struct
{
    int id;
    int irq;
    u32 base_addr;
    void (*callback)(void *user_data, u32 event);
    void *user_data;
} spi_pl_slave_bus_t;
#define SPI_PL_SLAVE_FIFO_DEPTH (1024)
#define SPI_PL_SLAVE_BUS_EVENT_RECEIVE_DONE 0   //BIT0


/************************** Function Prototypes ****************************/
void spi_pl_slave_bus_bind_irq_callback(spi_pl_slave_bus_t *bus, void (*callback)(void *user_data, u32 event),void *user_data);
void spi_pl_slave_bus_enable(spi_pl_slave_bus_t *bus, bool enable);
void spi_pl_slave_bus_reset_fifo(spi_pl_slave_bus_t *bus);
size_t spi_pl_slave_bus_recv(spi_pl_slave_bus_t *bus, uint8_t *RecvBuf[5], u32 ByteCount);
spi_pl_slave_bus_t *spi_pl_slave_bus_get_handle(int id);
void spi_pl_slave_bus_init(void);
size_t spi_pl_slave_bus_write(spi_pl_slave_bus_t *bus, uint8_t *SendBuf[5], u32 ByteCount);
void spi_pl_slave_bus_start_transfer(spi_pl_slave_bus_t *bus);
size_t spi_pl_slave_bus_recv_data4(spi_pl_slave_bus_t *bus, uint8_t *RecvBuf, u32 ByteCount);
void spi_pl_slave_bus_reset_rxfifo(spi_pl_slave_bus_t *bus);
#ifdef __cplusplus
}
#endif

#endif /* __SPI_PL_H__ */
/** @} */
