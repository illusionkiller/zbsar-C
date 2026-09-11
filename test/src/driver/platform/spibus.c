/***************************** Include Files *********************************/
#include "platform.h"
#include "xspi.h"        /* SPI device driver */
#include "spibus.h"


extern XIntc XIntcInstance;
#define InterruptController XIntcInstance
/************************** Constant Definitions *****************************/


typedef struct {
    int bus_id;
    int irq;
    SemaphoreHandle_t lock;
    XSpi handle;
    TaskHandle_t xTaskToNotify;
    int status;
} spibus_t;

static spibus_t spibus[] = {
    {
        .bus_id = XPAR_SPI_0_DEVICE_ID,
        .irq = XPAR_AXI_INTC_0_AXI_QUAD_SPI_0_IP2INTC_IRPT_INTR,
    }
};

/*****************************************************************************/
/**
 *
 * This function is the handler which performs processing for the QSPI driver.
 * It is called from an interrupt context such that the amount of processing
 * performed should be minimized.  It is called when a transfer of QSPI data
 * completes or an error occurs.
 *
 * This handler provides an example of how to handle QSPI interrupts but is
 * application specific.
 *
 * @param	CallBackRef is a reference passed to the handler.
 * @param	StatusEvent is the status of the QSPI .
 * @param	ByteCount is the number of bytes transferred.
 *
 * @return	None
 *
 * @note		None.
 *
 ******************************************************************************/
void SpiHandler(void *CallBackRef, u32 StatusEvent, unsigned int ByteCount)
{
   BaseType_t xHigherPriorityTaskWoken = pdFALSE;
   spibus_t *instance = (spibus_t *)CallBackRef;
   /*
    * Indicate the transfer on the QSPI bus is no longer in progress
    * regardless of the status event
    */
   if (StatusEvent == XST_SPI_TRANSFER_DONE)
   {
       if (instance->xTaskToNotify)
       {
           /* Notify the task that data has been sent. */
           vTaskNotifyGiveFromISR(instance->xTaskToNotify, &xHigherPriorityTaskWoken);

           portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
       }
   }
}

/*****************************************************************************/
/**
*
* This function setups the interrupt system such that interrupts can occur
* for the Spi device. This function is application specific since the actual
* system may or may not have an interrupt controller. The Spi device could be
* directly connected to a processor without an interrupt controller.  The
* user should modify this function to fit the application.
*
* @param	SpiPtr is a pointer to the instance of the Spi device.
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None
*
******************************************************************************/
static int SetupInterruptSystem(int bus_id)
{

	int Status;
	/*
	 * Initialize the interrupt controller driver so that
	 * it's ready to use, specify the device ID that is generated in
	 * xparameters.h
	 */
    // Status = XIntc_Initialize(&InterruptController, XPAR_INTC_0_DEVICE_ID);
    // if(Status != XST_SUCCESS) {
	// 	return XST_FAILURE;
	// }

	/*
	 * Connect a device driver handler that will be called when an interrupt
	 * for the device occurs, the device driver handler performs the
	 * specific interrupt processing for the device
	 */
	Status = XIntc_Connect(&InterruptController,
				spibus[bus_id].irq,
				(XInterruptHandler)XSpi_InterruptHandler,
				(void *)&spibus[bus_id].handle);
	if(Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Start the interrupt controller such that interrupts are enabled for
	 * all devices that cause interrupts, specific real mode so that
	 * the SPI can cause interrupts through the interrupt controller.
	 */
	// Status = XIntc_Start(&InterruptController, XIN_REAL_MODE);
	// if(Status != XST_SUCCESS) {
	// 	return XST_FAILURE;
	// }

	/*
	 * Enable the interrupt for the SPI.
	 */
	XIntc_Enable(&InterruptController, spibus[bus_id].irq);


	// /*
	//  * Initialize the exception table.
	//  */
	// Xil_ExceptionInit();

	// /*
	//  * Register the interrupt controller handler with the exception table.
	//  */
	// Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
	// 			(Xil_ExceptionHandler)XIntc_InterruptHandler,
	// 			&InterruptController);

	// /*
	//  * Enable non-critical exceptions.
	//  */
	// Xil_ExceptionEnable();

	return XST_SUCCESS;
}


uint32_t spibus_transfer(int bus_id, uint8_t *WriteBuffer, uint8_t *ReadBuffer, uint32_t ByteCount, uint32_t timeout_ms)
{
    u32 ret = 0;
    if (bus_id >= sizeof(spibus) / sizeof(spibus[0]))
    {
        return 0;
    }
    xSemaphoreTake(spibus[bus_id].lock, portMAX_DELAY);
    spibus[bus_id].xTaskToNotify = xTaskGetCurrentTaskHandle();
    if (XSpi_Transfer(&spibus[bus_id].handle, WriteBuffer, ReadBuffer, ByteCount) == XST_SUCCESS)
    {
        if (ulTaskNotifyTake(pdTRUE, timeout_ms) == 0)
        {
            ret = 0;
        }
        else
        {
            ret = ByteCount;
        }
    }
    else
    {
        ret = 0;
    }
    spibus[bus_id].xTaskToNotify = NULL;
    xSemaphoreGive(spibus[bus_id].lock);
    return ret;
}

void spi_bus_select(int bus_id, uint8_t slave_cs)
{
    if (bus_id >= sizeof(spibus) / sizeof(spibus[0]))
    {
        return;
    }
    uint32_t mask = (1 << slave_cs);
    xSemaphoreTake(spibus[bus_id].lock, portMAX_DELAY);
    XSpi_SetSlaveSelect(&spibus[bus_id].handle, mask);
    xSemaphoreGive(spibus[bus_id].lock);
}



void spibus_init(void)
{
    /*
        * Initialize the SPI driver so that it's ready to use,
        * specify the device ID that is generated in xparameters.h.
        */
    for (int i = 0; i < sizeof(spibus) / sizeof(spibus[0]); i++)
    {

        int Status = XSpi_Initialize(&spibus[i].handle, spibus[i].bus_id);
        if (Status != XST_SUCCESS)
        {
            return;
        }
        XSpi_Reset(&spibus[i].handle);
        Status = XSpi_SelfTest(&spibus[i].handle);
        if (Status != XST_SUCCESS)
        {
            return;
        }
        Status = SetupInterruptSystem(spibus[i].bus_id);
        if (Status != XST_SUCCESS)
        {
            return;
        }
        XSpi_SetStatusHandler(&spibus[i].handle, &spibus[i], (XSpi_StatusHandler)SpiHandler);
        Status = XSpi_SetOptions(&spibus[i].handle, XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION);
        if (Status != XST_SUCCESS)
        {
            return;
        }
        spibus[i].lock = xSemaphoreCreateMutex();
        configASSERT(spibus[i].lock != NULL);
        XSpi_Start(&spibus[i].handle);
        printf("spi bus[%d] init success\n", i);
    }

}
