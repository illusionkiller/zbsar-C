#include "platform.h"
#include "xgpio.h"
#include "pin.h"
/**************************** Type Definitions *******************************/

static XGpio Gpio;


void pin_init(void)
{
    int Status;
    /* Initialize the Gpio driver. */
    XGpio_Initialize(&Gpio, XPAR_GPIO_0_DEVICE_ID);
    /* Run a self-test on the GPIO device. */
    Status = XGpio_SelfTest(&Gpio);
    if (Status != XST_SUCCESS)
    {
        return;
    }
    /* Set the direction for all pins as inputs */
    XGpio_SetDataDirection(&Gpio, 1, 0);
    XGpio_DiscreteWrite(&Gpio, 1, 0);
    printf("pin dev init success\r\n");
}

bool pin_read(uint8_t pin_num)
{
    u32 value = XGpio_DiscreteRead(&Gpio, 1);
    return GET_BIT(value, pin_num);
}
void pin_write(uint8_t pin_num, bool value)
{
    if(value)
    {
        XGpio_DiscreteSet(&Gpio, 1, 1 << pin_num);
    }
    else
    {
        XGpio_DiscreteClear(&Gpio, 1, 1 << pin_num);
    }
}

void pin_output(uint8_t pin_num)
{
    uint32_t Direction_mask = XGpio_GetDataDirection(&Gpio, 1);
    SET0_BIT(Direction_mask, pin_num);
    XGpio_SetDataDirection(&Gpio, 1, Direction_mask);
}

void pin_input(uint8_t pin_num)
{
    uint32_t Direction_mask = XGpio_GetDataDirection(&Gpio, 1);
    SET1_BIT(Direction_mask, pin_num);
    XGpio_SetDataDirection(&Gpio, 1, Direction_mask);
}
