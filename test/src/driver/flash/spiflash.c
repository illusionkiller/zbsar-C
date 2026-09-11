/***************************** Include Files *********************************/
#include "platform.h"
#include "spibus.h"
#include "spiflash.h"
#define SPI_FLASH_USE_S25FL256
#ifdef SPI_FLASH_USE_S25FL256
typedef union
{
    struct
    {
        uint8_t BUSY : 1;
        uint8_t WEL : 1;
        uint8_t BP0 : 1;
        uint8_t BP1 : 1;
        uint8_t BP2 : 1;
        uint8_t E_ERR : 1;
        uint8_t P_ERR : 1;
        uint8_t SRWD : 1;
    };
    uint8_t byte;
} status1;

typedef union
{
    struct
    {
        uint8_t PS : 1;
        uint8_t ES : 1;
        uint8_t RESERVED : 6;
    };
    uint8_t byte;
} status2;

typedef union
{
    struct
    {
        uint8_t FREEZE : 1;
        uint8_t QE : 1;
        uint8_t TBPARM : 1; // 4-KB physical sectors at bottom
        uint8_t BPNV : 1;
        uint8_t DNU : 1;
        uint8_t TBPROT : 1;
        uint8_t LC0 : 1;
        uint8_t LC1 : 1;
    };
    uint8_t byte;
} cr_reg_t;

typedef union
{
    struct
    {
        uint8_t BA24 : 1;
        uint8_t RESERVED : 6;
        uint8_t EXTADD : 1;
    };
    uint8_t byte;
} bar_reg_t;
typedef struct
{
    int dev_id;
    int bus_id;
    int bus_cs;
    SemaphoreHandle_t lock;
    status1 status1_reg;
    status2 status2_reg;
    cr_reg_t cr_reg;
    bar_reg_t bar_reg;

} spiflash_dev_t;


#define SPI_FLASH_PAGE_SIZE 256
#define SPI_FLASH_BLOCK32K_SIZE 0x8000  // 32KB
#define SPI_FLASH_BLOCK64K_SIZE 0x10000 // 64KB
#define CMD_WRITE_ENABLE 0x06
#define CMD_WRITE_DISABLE 0x04
#define CMD_READ_STATUS1_REGISTER 0x05
#define CMD_CLEAR_STATUS1_REGISTER 0x30

#define CMD_READ_STATUS2_REGISTER 0x07
#define CMD_READ_CR1_REGISTER 0x35
#define CMD_READ_BAR_REGISTER 0x16
#define CMD_WRITE_BAR_REGISTER 0x17

#define CMD_WRITE_REGISTER 0x01

#define CMD_READ_DATA 0x03
#define CMD_FAST_READ_DATA 0x0B
#define CMD_DUAL_READ_DATA 0x3B
#define CMD_DUAL_IO_FAST_READ 0xBB /* Dual IO Fast Read */
#define CMD_QUAD_READ_DATA 0x6B
#define CMD_QUAD_IO_FAST_READ 0xEB /* Quad IO Fast Read */

#define CMD_PAGE_PROGRAM 0x02
#define CMD_QUAD_WRITE 0x32 /* Quad Input Fast Program */
#define CMD_SECTOR_ERASE 0x20
#define CMD_BLOCK_ERASE_64K 0xD8
#define CMD_CHIP_ERASE 0xC7

#define CMD_READ_ID 0x90
#define CMD_RESET 0xF0
#define DUMMY_BYTE 0x00
#endif



static spiflash_dev_t spiflash_devs[] = {
    {
        .dev_id = 0,
        .bus_id = 0,
        .bus_cs = 0,
    },
};


static int flash_write_enable(int dev_id, bool enable)
{
    uint8_t write_buf[1];
    write_buf[0] =  (enable) ? CMD_WRITE_ENABLE : CMD_WRITE_DISABLE;
    uint32_t size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, NULL, 1, 100);
    if (size != 1)
    {
        return -1;
    }
    return 0;
}

static int flash_write_registers(int dev, uint8_t SR1_reg, uint8_t CR_reg)
{
    int ret = flash_write_enable(dev, true);
    if (ret != 0)
    {
        return ret;
    }
    uint8_t write_buf[2];
    write_buf[0] = CMD_WRITE_REGISTER;
    write_buf[1] = SR1_reg;
    write_buf[2] = CR_reg;
    uint32_t size = spibus_transfer(spiflash_devs[dev].bus_id, write_buf, NULL, 3, 100);
    if (size != 3)
    {
        return -1;
    }
    spiflash_devs[dev].status1_reg.byte = SR1_reg;
    spiflash_devs[dev].cr_reg.byte = CR_reg;
    return 0;
}

static int flash_read_status(int dev_id, uint8_t reg_index, uint8_t *status)
{
    uint8_t write_buf[2];
    switch (reg_index)
    {
    case 1:
        write_buf[0] = CMD_READ_STATUS1_REGISTER;
        break;
    case 2:
        write_buf[0] = CMD_READ_STATUS2_REGISTER;
        break;
    default:
        return -1;
    }
    write_buf[1] = DUMMY_BYTE;
    uint32_t size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, write_buf, 2, 100);
    if (size != 2)
    {
        return -1;
    }
    *status = write_buf[1];
    return 0;
}

static int flash_clear_status(int dev_id)
{
    uint8_t write_buf[1];
    write_buf[0] = CMD_CLEAR_STATUS1_REGISTER;
    uint32_t size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, NULL, 1, 100);
    if (size != 1)
    {
        return -1;
    }
    return 0;
}

// 读取CR寄存器
static int flash_read_cr(int dev_id,uint8_t *cr)
{
    if (dev_id >= sizeof(spiflash_devs) / sizeof(spiflash_devs[0]))
    {
        return -1;
    }
    uint8_t write_buf[2];
    write_buf[0] = CMD_READ_CR1_REGISTER;
    write_buf[1] = DUMMY_BYTE;
    uint32_t size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, write_buf, 2, 100);
    if (size != 2)
    {
        return -1;
    }
    *cr = write_buf[1];
    return 0;
}

static int flash_read_bar(int dev_id, uint8_t *bar)
{
    uint8_t write_buf[2];
    write_buf[0] = CMD_READ_BAR_REGISTER;
    write_buf[1] = DUMMY_BYTE;
    uint32_t size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, write_buf, 2, 100);
    if (size != 2)
    {
        return -1;
    }
    *bar = write_buf[1];
    return 0;
}

static int flash_write_bar(int dev_id, uint8_t bar)
{
    uint8_t write_buf[2];
    write_buf[0] = CMD_WRITE_BAR_REGISTER;
    write_buf[1] = bar;
    uint32_t size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, NULL, 2, 100);
    if (size != 2)
    {
        return -1;
    }
    return 0;
}

static int wait_flash_idle(int dev_id, uint32_t timeout_ms)
{
    status1 status = {
        .BUSY = 1,
        .WEL = 1,
        .P_ERR = 1,
    };
    while (timeout_ms--)
    {
        vTaskDelay(pdMS_TO_TICKS(1));
        int ret = flash_read_status(dev_id, 1, &status.byte);
        if (ret == 0)
        {
            if (status.BUSY == 0 && status.WEL == 0 && status.P_ERR == 0)
            {
                break;
            }
        }
    }
    if (timeout_ms == 0)
    {
        printf("wait flash idle timeout\r\n");
        return -1;
    }
    return 0;
}


int flash_4B_addr_enable(int dev_id, bool enable)
{
    bar_reg_t bar_reg;
    int ret = flash_read_bar(dev_id, &bar_reg.byte);
    if (ret != 0)   
    {
        return ret;
    }
    bar_reg.EXTADD = enable ? 1 : 0;
    ret = flash_write_bar(dev_id, bar_reg.byte);
    if (ret != 0)
    {
        return ret;
    }
    spiflash_devs[dev_id].bar_reg = bar_reg;
    return 0;
}

uint32_t flash_read_data(int dev_id, uint32_t addr, uint8_t *data, uint32_t size, uint32_t timeout_ms)
{
    uint8_t cmd_size = 0;
    uint32_t transfer_size = 0;
    uint8_t *transfer_buf = NULL;
    if (spiflash_devs[dev_id].bar_reg.EXTADD == 0)
    {
        if (addr > 0xFFFFFF)
        {
            printf("addr 0x%08lx(>0xFFFFFF) is not support in 3-byte address mode\r\n", addr);
            return 0;
        }
        else
        {
            cmd_size = 4;
            transfer_size = cmd_size + size;
            transfer_buf = pvPortMalloc(transfer_size);
            configASSERT(transfer_buf);
            transfer_buf[0] = CMD_READ_DATA; // 3-byte address read data command
            transfer_buf[1] = (uint8_t)((addr & 0xFF0000) >> 16);
            transfer_buf[2] = (uint8_t)((addr & 0xFF00) >> 8);
            transfer_buf[3] = (uint8_t)(addr & 0xFF);
        }
    }
    else
    {
        cmd_size = 5;
        transfer_size = cmd_size + size;
        transfer_buf = pvPortMalloc(transfer_size);
        configASSERT(transfer_buf);
        transfer_buf[0] = CMD_READ_DATA;
        transfer_buf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
        transfer_buf[2] = (uint8_t)((addr & 0xFF0000) >> 16);
        transfer_buf[3] = (uint8_t)(addr & 0xFF00 >> 8);
        transfer_buf[4] = (uint8_t)(addr & 0xFF);
    }
    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    uint32_t ret_size = spibus_transfer(spiflash_devs[dev_id].bus_id, transfer_buf, transfer_buf, transfer_size, timeout_ms);
    if (ret_size == transfer_size)
    {
        memcpy(data, transfer_buf + cmd_size, size);
        ret_size = size;
    }
    else
    {
        ret_size = 0;
    }
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    vPortFree(transfer_buf);
    return ret_size;
}

uint32_t flash_fast_read_data(int dev_id, uint32_t addr, uint8_t *data, uint32_t size, uint32_t timeout_ms)
{
    uint8_t cmd_size = 0;
    uint32_t transfer_size = 0;
    uint8_t *transfer_buf = NULL;
    if (spiflash_devs[dev_id].bar_reg.EXTADD == 0)
    {
        if (addr > 0xFFFFFF)
        {
            printf("addr 0x%08lx(>0xFFFFFF) is not support in 3-byte address mode\r\n", addr);
            return 0;
        }
        else
        {
            cmd_size = 5;
            transfer_size = cmd_size + size;
            transfer_buf = pvPortMalloc(transfer_size);
            configASSERT(transfer_buf);
            transfer_buf[0] = CMD_FAST_READ_DATA; // 3-byte address read data command
            transfer_buf[1] = (uint8_t)((addr & 0xFF0000) >> 16);
            transfer_buf[2] = (uint8_t)((addr & 0xFF00) >> 8);
            transfer_buf[3] = (uint8_t)(addr & 0xFF);
            transfer_buf[4] = DUMMY_BYTE;
        }
    }
    else
    {
        cmd_size = 6;
        transfer_size = cmd_size + size;
        transfer_buf = pvPortMalloc(transfer_size);
        configASSERT(transfer_buf);
        transfer_buf[0] = CMD_FAST_READ_DATA;
        transfer_buf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
        transfer_buf[2] = (uint8_t)((addr & 0xFF0000) >> 16);
        transfer_buf[3] = (uint8_t)(addr & 0xFF00 >> 8);
        transfer_buf[4] = (uint8_t)(addr & 0xFF);
        transfer_buf[5] = DUMMY_BYTE;
    }
    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    uint32_t ret_size = spibus_transfer(spiflash_devs[dev_id].bus_id, transfer_buf, transfer_buf, transfer_size, timeout_ms);
    if (ret_size == transfer_size)
    {
        memcpy(data, transfer_buf + cmd_size, size);
        ret_size = size;
    }
    else
    {
        ret_size = 0;
    }
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    vPortFree(transfer_buf);
    return ret_size;
}
// S25FL256SAGNFI001  Uniform 64-KB sectors
// Sector and Memory Address Map, Bottom 4-KB Sector
// A hybrid of 32 x 4-KB sectors with all remaining sectors being 64 KB, with a 256B programming buffer.
uint32_t flash_page_program(int dev_id, uint32_t addr, uint8_t *data, uint32_t size, uint32_t timeout_ms)
{
    uint8_t cmd_size = 0;
    uint32_t transfer_size  = 0;
    uint32_t allow_write_size = SPI_FLASH_PAGE_SIZE - (addr & 0x000000FF);
    uint32_t write_size = size > allow_write_size ? allow_write_size : size;
    if (write_size == 0) return 0;  
    uint8_t *transfer_buf = NULL;
    if (spiflash_devs[dev_id].bar_reg.EXTADD == 0) // 3-byte address page program command
    {
        if (addr > 0xFFFFFF)
        {
            printf("addr 0x%08lx(>0xFFFFFF) is not support in 3-byte address mode\r\n", addr);
            return 0;
        }
        cmd_size = 4;
        transfer_size = cmd_size + write_size;
        transfer_buf = pvPortMalloc(transfer_size);
        configASSERT(transfer_buf);
        transfer_buf[0] = CMD_PAGE_PROGRAM; 
        transfer_buf[1] = (uint8_t)((addr & 0xFF0000) >> 16);
        transfer_buf[2] = (uint8_t)((addr & 0xFF00) >> 8);
        transfer_buf[3] = (uint8_t)(addr & 0xFF);
        memcpy(&transfer_buf[cmd_size], data, write_size);
    }
    else // 4-byte address page program command
    {
        cmd_size = 5;
        transfer_size = cmd_size + write_size;
        transfer_buf = pvPortMalloc(transfer_size);
        configASSERT(transfer_buf);
        transfer_buf[0] = CMD_PAGE_PROGRAM;
        transfer_buf[1] = (uint8_t)((addr & 0xFF000000) >> 24);
        transfer_buf[2] = (uint8_t)((addr & 0xFF0000) >> 16);
        transfer_buf[3] = (uint8_t)((addr & 0xFF00) >> 8);
        transfer_buf[4] = (uint8_t)(addr & 0xFF);
        memcpy(&transfer_buf[cmd_size], data, write_size);
    }
    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    int ret = flash_write_enable(dev_id, true);
    if (ret != 0)
    {
        xSemaphoreGive(spiflash_devs[dev_id].lock);
        vPortFree(transfer_buf);
        printf("flash write enable failed\r\n");
        return 0;
    }
    uint32_t ret_size = spibus_transfer(spiflash_devs[dev_id].bus_id, transfer_buf, NULL, transfer_size, timeout_ms);
    if (ret_size == transfer_size)
    {
        ret_size = write_size;
        ret = wait_flash_idle(dev_id, 10); // wait for program done 3ms
        if (ret != 0)
        {
            ret_size = 0;
        }
    }
    else
    {
        ret_size = 0;
    }
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    vPortFree(transfer_buf);
    return ret_size;
}

int flash_erase_sector(int dev_id, uint32_t addr)
{
    uint32_t transfer_size = 0;
    uint8_t write_buf[5] = {0};
    uint32_t erase_addr = addr - (addr % SPI_FLASH_SECTOR_SIZE);
    if (addr > 0x1FFFF)  //128k
    {
        printf("S25FL256S Sector and Memory Address Map, Bottom 4-KB Sector, addr 0x%08lx is > 0x1FFFF not support\r\n", addr);
        return -1;
    }
    write_buf[0] = CMD_SECTOR_ERASE;
    write_buf[1] = (uint8_t)((erase_addr & 0xFF0000) >> 16);
    write_buf[2] = (uint8_t)((erase_addr & 0xFF00) >> 8);
    write_buf[3] = (uint8_t)(erase_addr & 0xFF);
    transfer_size = 4;

    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    int ret = flash_write_enable(dev_id, true);
    if (ret != 0)
    {
        xSemaphoreGive(spiflash_devs[dev_id].lock);
        printf("flash write enable failed\r\n");
        return ret;
    }

    uint32_t ret_size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, NULL, transfer_size, 100);
    if (ret_size == transfer_size)
    {
        ret = wait_flash_idle(dev_id, 400); // wait for erase done
        xSemaphoreGive(spiflash_devs[dev_id].lock);
        return ret;
    }
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    return -1;
}

int flash_erase_block64K(int dev_id, uint32_t addr)
{
    if (dev_id >= sizeof(spiflash_devs) / sizeof(spiflash_devs[0]))
    {
        return -1;
    }
    uint32_t transfer_size = 0;
    uint32_t erase_addr = addr - (addr % SPI_FLASH_BLOCK64K_SIZE);
    uint8_t write_buf[5] = {0};
    if (spiflash_devs[dev_id].bar_reg.EXTADD == 0) // 3-byte address erase sector command
    {
        if (addr > 0xFFFFFF)
        {
            printf("addr 0x%08lx(>0xFFFFFF) is not support in 3-byte address mode\r\n", addr);
            return -1;
        }
        write_buf[0] = CMD_BLOCK_ERASE_64K;
        write_buf[1] = (uint8_t)((erase_addr & 0xFF0000) >> 16);
        write_buf[2] = (uint8_t)((erase_addr & 0xFF00) >> 8);
        write_buf[3] = (uint8_t)(erase_addr & 0xFF);
        transfer_size = 4;
    }
    else // 4-byte address erase sector command
    {
        write_buf[0] = CMD_BLOCK_ERASE_64K;
        write_buf[1] = (uint8_t)((erase_addr & 0xFF000000) >> 24);
        write_buf[2] = (uint8_t)((erase_addr & 0xFF0000) >> 16);
        write_buf[3] = (uint8_t)((erase_addr & 0xFF00) >> 8);
        write_buf[4] = (uint8_t)(erase_addr & 0xFF);
        transfer_size = 5;
    }
    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    int ret = flash_write_enable(dev_id, true);
    if (ret != 0)
    {
        xSemaphoreGive(spiflash_devs[dev_id].lock);
        printf("flash write enable failed\r\n");
        return ret;
    }
    uint32_t ret_size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, NULL, transfer_size, 100);
    if (ret_size == transfer_size)
    {
        ret = wait_flash_idle(dev_id, 10000); 
        xSemaphoreGive(spiflash_devs[dev_id].lock);
        return ret;
    }
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    return -1;
}

int flash_erase_chip(int dev_id)
{
    if (dev_id >= sizeof(spiflash_devs) / sizeof(spiflash_devs[0]))
    {
        return -1;
    }
    int ret = flash_write_enable(dev_id, true);
    if (ret != 0)
    {
        printf("flash write enable failed\r\n");
        return ret;
    }
    uint8_t write_buf[1] = {CMD_CHIP_ERASE};
    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    uint32_t ret_size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, NULL, sizeof(write_buf), 100);
    if (ret_size == sizeof(write_buf))
    {
        ret = wait_flash_idle(dev_id, 200000); // 200s
        xSemaphoreGive(spiflash_devs[dev_id].lock);
        return ret;
    }
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    return -1;
}

int flash_recover_standby(int dev_id)
{
    flash_clear_status(dev_id);
    return flash_write_enable(dev_id,false);
}

int flash_soft_reset(int dev_id)
{
    if (dev_id >= sizeof(spiflash_devs) / sizeof(spiflash_devs[0]))
    {
        return -1;
    }
    uint8_t write_buf[1] = {CMD_RESET};
    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, NULL, sizeof(write_buf), 100);
    vTaskDelay(pdMS_TO_TICKS(200));
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    return -1;
}

int flash_read_id(int dev_id, uint8_t id_buf[2])
{
    if (dev_id >= sizeof(spiflash_devs) / sizeof(spiflash_devs[0]))
    {
        return -1;
    }
    uint8_t write_buf[6] = {CMD_READ_ID, DUMMY_BYTE, DUMMY_BYTE, DUMMY_BYTE};
    xSemaphoreTake(spiflash_devs[dev_id].lock, portMAX_DELAY);
    uint32_t ret_size = spibus_transfer(spiflash_devs[dev_id].bus_id, write_buf, write_buf, sizeof(write_buf), 100);
    if (ret_size == sizeof(write_buf))
    {
        memcpy(id_buf, &write_buf[4], 2);
        xSemaphoreGive(spiflash_devs[dev_id].lock);
        return 0;
    }
    xSemaphoreGive(spiflash_devs[dev_id].lock);
    return -1;
}

void spi_flash_init(void)
{
	spibus_init();
    uint8_t id_buf[2] = {0};
    for (int dev_id = 0; dev_id < sizeof(spiflash_devs) / sizeof(spiflash_devs[0]); dev_id++)
    {
        spiflash_devs[dev_id].lock = xSemaphoreCreateMutex();
        configASSERT(spiflash_devs[dev_id].lock);
        spi_bus_select(spiflash_devs[dev_id].bus_id , spiflash_devs[dev_id].bus_cs);
        flash_soft_reset(dev_id);
        if (flash_read_id(dev_id, id_buf) == 0)
        {
            flash_4B_addr_enable(dev_id, true); // 使能4字节地址模式
            flash_read_status(dev_id, 1, &spiflash_devs[dev_id].status1_reg.byte);
            flash_read_status(dev_id, 2, &spiflash_devs[dev_id].status2_reg.byte);
            flash_read_cr(dev_id, &spiflash_devs[dev_id].cr_reg.byte);
            flash_read_bar(dev_id, &spiflash_devs[dev_id].bar_reg.byte);
            printf("flash %d initial success, id: %02x %02x\r\n", dev_id, id_buf[0], id_buf[1]);
        }
    }
}












