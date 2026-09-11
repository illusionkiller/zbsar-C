#include "platform.h"
#include "pin.h"
#include "smi_switch.h"
#define SMI_ST_CODE 0x1
#define SMI_OP_READ 0x2
#define SMI_OP_WRITE 0x1
#define SMI_TA_CODE 0x2
#define REG_ADDR_BIT1_ADDR 0
#define REG_ADDR_BIT1_DATA 1
#define REG_ADDR_BIT0_WRITE 0
#define REG_ADDR_BIT0_READ 1

#define PHYADDR 0x1D /*base on Hardware Switch Phyaddr*/
#define SWITCHID 0x0 /*base on Hardware Switch SwitchID*/
#define DEFAULT_SWITCH_ID 0x90020001

#define Regaddr_id 0x80008
#define Regaddr_serdes_sw 0x80394
#define Regaddr_serdes_en 0x80028
#define Regaddr_xmii_en 0x80408
#define Regaddr_xmii_speed 0x80124
#define Regaddr_serdes0_mode 0x8008c
#define Regaddr_serdes1_mode 0x80090


static void delay_us(uint32_t us)
{
	usleep(us);
}

typedef struct 
{
    uint8_t smi_mdc_pin;
    uint8_t smi_mdio_pin;
    uint8_t smi_rst_pin;
}smi_switch_dev_t;

static smi_switch_dev_t smi_switch_dev[] = {
    {
        .smi_mdc_pin = 1,
        .smi_mdio_pin = 2,
        .smi_rst_pin = 3,
    },
    {
        .smi_mdc_pin = 4,
        .smi_mdio_pin = 5,
        .smi_rst_pin = 6,
    }
};

void smi_mdc_output(int bus_id)
{
    pin_output(smi_switch_dev[bus_id].smi_mdc_pin);
}

void smi_mdio_output(int bus_id)
{
    pin_output(smi_switch_dev[bus_id].smi_mdio_pin);
}

void smi_mdio_input(int bus_id)
{
    pin_input(smi_switch_dev[bus_id].smi_mdio_pin);
}


void smi_switch_reset(int bus_id)
{
    pin_output(smi_switch_dev[bus_id].smi_rst_pin);
//    pin_write(smi_switch_dev[bus_id].smi_rst_pin, 0);
//    vTaskDelay(20 / portTICK_RATE_MS);
    pin_write(smi_switch_dev[bus_id].smi_rst_pin, 1);  // 复位高电平
    vTaskDelay(100 / portTICK_RATE_MS);  //min reset time 10ms
    // sleep(1);
    pin_write(smi_switch_dev[bus_id].smi_rst_pin, 0);
    vTaskDelay(100 / portTICK_RATE_MS); 
}


void smi_write_bits(int bus_id, unsigned short data, uint32_t len)
{
    pin_output(smi_switch_dev[bus_id].smi_mdio_pin);
    for (; len > 0; len--)
    {
        pin_write(smi_switch_dev[bus_id].smi_mdc_pin, 0);
        delay_us(1);
        if (data & (1 << (len - 1)))
        {
            pin_write(smi_switch_dev[bus_id].smi_mdio_pin, 1);
        }
        else
        {
            pin_write(smi_switch_dev[bus_id].smi_mdio_pin, 0);
        }
        delay_us(1);
        pin_write(smi_switch_dev[bus_id].smi_mdc_pin, 1);
        delay_us(1);
    }
    pin_write(smi_switch_dev[bus_id].smi_mdc_pin, 0);
}

void smi_read_bits(int bus_id, uint32_t len, unsigned short *data)
{
    unsigned short tmp = 0;
    pin_input(smi_switch_dev[bus_id].smi_mdio_pin);
    for (*data = 0; len > 0; len--)
    {
        tmp = pin_read(smi_switch_dev[bus_id].smi_mdio_pin);
        *data |= ((tmp & 0x1) << (len - 1));
        delay_us(1);
        pin_write(smi_switch_dev[bus_id].smi_mdc_pin, 1);
        delay_us(1);
        pin_write(smi_switch_dev[bus_id].smi_mdc_pin, 0);
        delay_us(1);
    }
}

uint32_t smi_write_cl22(int bus_id, uint8_t phyAddr, uint8_t regAddr, unsigned short regVal)
{
    smi_write_bits(bus_id, 0xffff, 16);
    smi_write_bits(bus_id, 0xffff, 16);
    smi_write_bits(bus_id, SMI_ST_CODE, 2);
    smi_write_bits(bus_id, SMI_OP_WRITE, 2);
    smi_write_bits(bus_id, phyAddr, 5);
    smi_write_bits(bus_id, regAddr, 5);
    smi_write_bits(bus_id, SMI_TA_CODE, 2);
    smi_write_bits(bus_id, regVal, 16);
    return 0;
}

uint32_t smi_read_cl22(int bus_id, uint8_t phyAddr, uint8_t regAddr, unsigned short *pRegVal)
{
    smi_write_bits(bus_id, 0xffff, 16);
    smi_write_bits(bus_id, 0xffff, 16);
    smi_write_bits(bus_id, SMI_ST_CODE, 2);
    smi_write_bits(bus_id, SMI_OP_READ, 2);
    smi_write_bits(bus_id, phyAddr, 5);
    smi_write_bits(bus_id, regAddr, 5);
    smi_read_bits(bus_id, 2, pRegVal);
    smi_read_bits(bus_id, 16, pRegVal);
    return 0;
}

uint32_t smi_switch_write(int bus_id, uint8_t phyAddr, uint8_t switchId, uint32_t reg_addr, uint32_t reg_value)
{
    uint8_t regAddr;
    unsigned short regVal;
    //	xil_printf("%write addr:%x reg:%x\r\n",reg_addr,reg_value);
    regAddr = (switchId << 2) | (REG_ADDR_BIT1_ADDR << 1) | (REG_ADDR_BIT0_WRITE); // 0x0
    /* Set reg_addr[31:16] */
    regVal = (reg_addr >> 16) & 0xffff;
    smi_write_cl22(bus_id, phyAddr, regAddr, regVal);
    /* Set reg_addr[15:0] */
    regVal = reg_addr & 0xffff;
    smi_write_cl22(bus_id, phyAddr, regAddr, regVal);
    /* Write Data [31:16] out */
    regAddr = (switchId << 2) | (REG_ADDR_BIT1_DATA << 1) | (REG_ADDR_BIT0_WRITE); // 0x2
    regVal = (reg_value >> 16) & 0xffff;
    smi_write_cl22(bus_id, phyAddr, regAddr, regVal);
    /* Write Data [15:0] out */
    regVal = reg_value & 0xffff;
    smi_write_cl22(bus_id, phyAddr, regAddr, regVal);
    vTaskDelay(10 / portTICK_RATE_MS);
    return 0;
}

uint32_t smi_switch_read(int bus_id, uint8_t phyAddr, uint8_t switchId, uint32_t reg_addr, uint32_t *reg_value)
{
    uint32_t rData;
    uint8_t regAddr;
    unsigned short regVal;
    regAddr = (switchId << 2) | (REG_ADDR_BIT1_ADDR << 1) | (REG_ADDR_BIT0_READ); // 0x1
    /* Set reg_addr[31:16] */
    regVal = (reg_addr >> 16) & 0xffff;
    smi_write_cl22(bus_id, phyAddr, regAddr, regVal);
    // delay_us(1);
    /* Set reg_addr[15:0] */
    regVal = reg_addr & 0xffff;
    smi_write_cl22(bus_id, phyAddr, regAddr, regVal);
    regAddr = (switchId << 2) | (REG_ADDR_BIT1_DATA << 1) | (REG_ADDR_BIT0_READ); // 0x3
    // delay_us(1);
    /* Read Data [31:16] */
    regVal = 0x0;
    smi_read_cl22(bus_id, phyAddr, regAddr, &regVal);
    rData = (uint32_t)(regVal << 16);
    // delay_us(1);
    /* Read Data [15:0] */
    regVal = 0x0;
    smi_read_cl22(bus_id, phyAddr, regAddr, &regVal);
    rData |= regVal;
    // delay_us(1);
    *reg_value = rData;
    return 0;
}

uint32_t smi_switch_get_id(int bus_id)
{
    uint32_t reg_addr = Regaddr_id;
    uint32_t read_data = 0;
    smi_switch_read(bus_id, PHYADDR, SWITCHID ,reg_addr,&read_data);
    printf("SC8367 ID(%d) is : 0x%08lx\r\n",bus_id, read_data);
    return read_data;
}

static uint32_t switch_reg_init_table[][2] = {
    {Regaddr_serdes_en, 0x03},
    {Regaddr_serdes0_mode, 0xfA},
    {Regaddr_serdes1_mode, 0xfA},
    {0xF0004, 0x11E0804},
    {0xF0008, 0xA0},
    {0xF0000, 0x1},
    {0xF0004, 0x11F0804},
    {0xF0008, 0x8C00},
    {0xF0000, 0x1},
    {0xF0004, 0x13E0904},
    {0xF0008, 0xA0},
    {0xF0000, 0x1},
    {0xF0004, 0x13F0904},
    {0xF0008, 0x8C00},
    {0xF0000, 0x1},
};

void smi_switch_init(void)
{
    uint32_t reg_addr = 0;
    uint32_t reg_value = 0;
    vTaskDelay(2000 / portTICK_RATE_MS); // 等待电源稳定
    for (int bus_id = 0; bus_id < 1; bus_id++)
    {
        smi_switch_reset(bus_id);
        uint32_t switch_id = smi_switch_get_id(bus_id);
        if (switch_id == DEFAULT_SWITCH_ID)
        {
            for (int i = 0; i < sizeof(switch_reg_init_table) / sizeof(switch_reg_init_table[0]); i++)
            {
                reg_addr = switch_reg_init_table[i][0];
                reg_value = switch_reg_init_table[i][1];
                smi_switch_write(bus_id, PHYADDR, SWITCHID, reg_addr, reg_value);
                smi_switch_read(bus_id, PHYADDR, SWITCHID, reg_addr, &reg_value);
                printf("%lX : 0x%08lX\r\n", reg_addr, reg_value);
            }
        }
    }
}
