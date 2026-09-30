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
   volatile TaskHandle_t xTaskToNotify;
   volatile u32 status;
   volatile unsigned int transferred_bytes;
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
    /* Wake the owner for both completion and error events.  The task checks
     * the status instead of treating every notification as success. */
    instance->status = StatusEvent;
    instance->transferred_bytes = ByteCount;
    if (instance->xTaskToNotify != NULL)
    {
        vTaskNotifyGiveFromISR(instance->xTaskToNotify, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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


uint32_t spibus_transfer(void *handle, uint8_t *WriteBuffer, uint8_t *ReadBuffer, uint32_t ByteCount, uint32_t timeout_ms)
{
    uint32_t ret = 0U;
    TickType_t timeout_ticks;

    if(handle == NULL || ByteCount == 0U)
    {
        return 0;
    }
    spibus_t *instance = (spibus_t *)handle;

    if (xSemaphoreTake(instance->lock, portMAX_DELAY) != pdPASS)
    {
        return 0;
    }

    /* Discard a stale completion before arming this transaction. */
    (void)ulTaskNotifyTake(pdTRUE, 0U);
    instance->status = 0U;
    instance->transferred_bytes = 0U;
    /* Arm the waiter before starting the non-blocking hardware transfer. */
    instance->xTaskToNotify = xTaskGetCurrentTaskHandle();

    if (XSpi_Transfer(&instance->handle, WriteBuffer, ReadBuffer, ByteCount) == XST_SUCCESS)
    {
        timeout_ticks = pdMS_TO_TICKS(timeout_ms);
        if (timeout_ms != 0U && timeout_ticks == 0U)
        {
            timeout_ticks = 1U;
        }
        if (ulTaskNotifyTake(pdTRUE, timeout_ticks) != 0U &&
            instance->status == XST_SPI_TRANSFER_DONE &&
            instance->transferred_bytes == ByteCount)
        {
            ret = ByteCount;
        }
        else
        {
            /* Leave the controller idle for the next caller after timeout or
             * an error status.  The mutex is still held, so no new transfer
             * can race this recovery. */
			XSpi_Reset(&instance->handle);
			(void)XSpi_Start(&instance->handle);
			uint32_t selected_slave = XSpi_GetSlaveSelect(&instance->handle);
			XSpi_SetSlaveSelect(&instance->handle, selected_slave);
        }
    }
    /* A failed XSpi_Transfer() did not start a transaction. */
    instance->xTaskToNotify = NULL;
    xSemaphoreGive(instance->lock);
    return ret;
}

void spibus_select(void *handle, uint8_t slave_cs)
{
    if(handle == NULL)
    {
        return;
    }
    spibus_t *instance = (spibus_t *)handle;
    uint32_t mask = (1 << slave_cs);
    xSemaphoreTake(instance->lock, portMAX_DELAY);
    XSpi_SetSlaveSelect(&instance->handle, mask);   
    xSemaphoreGive(instance->lock);
}

void spibus_deselect(void *handle)
{
    if (handle == NULL)
    {
        return;
    }
    spibus_t *instance = (spibus_t *)handle;
    uint32_t mask  = 0;
    xSemaphoreTake(instance->lock, portMAX_DELAY);
    XSpi_SetSlaveSelect(&instance->handle, mask);
    xSemaphoreGive(instance->lock);
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
        Status = XSpi_SetOptions(&spibus[i].handle, XSP_MASTER_OPTION | XSP_MANUAL_SSELECT_OPTION); //mode 0
        if (Status != XST_SUCCESS)
        {
            return;
        }
//        uint32_t ctrl_reg = XSpi_GetControlReg(&spibus[i].handle);
//            ctrl_reg &= ~XSP_CR_LSB_MSB_FIRST_MASK;
//        XSpi_SetControlReg(&spibus[i].handle, ctrl_reg);
//
        spibus[i].lock = xSemaphoreCreateMutex();
        configASSERT(spibus[i].lock != NULL);
        XSpi_Start(&spibus[i].handle);
//    	XSpi_IntrGlobalDisable(&spibus[i].handle);
//    	XSpi_IntrGlobalEnable(&spibus[i].handle);
        printf("spi bus[%d] init success\n", i);
    }
}

void *get_spibus_handle(int bus_id)
{
    if(bus_id < 0 || bus_id >= sizeof(spibus) / sizeof(spibus[0]))
    {
        return NULL;
    }
    return &spibus[bus_id];
}
