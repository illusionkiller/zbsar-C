#include "platform.h"
#include "xuartlite.h"
#include "uartlite.h"

extern XIntc XIntcInstance;

#define UARTLITE_DEVICE_ID XPAR_UARTLITE_0_DEVICE_ID
#define UARTLITE_INT_IRQ_ID XPAR_AXI_INTC_0_AXI_UARTLITE_0_INTERRUPT_INTR
#define UARTLITE_RING_BUFFER_SIZE 1024U

typedef struct
{
    uint8_t data[UARTLITE_RING_BUFFER_SIZE];
    volatile size_t head;
    volatile size_t tail;
} uartlite_ring_t;

typedef struct
{
    uint16_t device_id;
    uint16_t interrupt_id;
    bool initialized;
    bool opened;
    XUartLite instance;
    uartlite_ring_t rx_ring;
    uint8_t cur_rx_byte;
    SemaphoreHandle_t rx_sem;
} uartlite_device_t;

static uartlite_device_t uartlite_device = {
    .device_id = UARTLITE_DEVICE_ID,
    .interrupt_id = UARTLITE_INT_IRQ_ID,
};

static void uartlite_ring_flush(uartlite_ring_t *ring)
{
    ring->head = ring->tail = 0;
}

// static size_t uartlite_ring_write(uartlite_ring_t *ring,
//                                 const uint8_t *data,
//                                 size_t len)
// {
//     size_t written = 0U;

//     while (written < len)
//     {
//         size_t next = (ring->head + 1U) % UARTLITE_RING_BUFFER_SIZE;
//         if (next == ring->tail)
//         {
//             break;
//         }
//         ring->data[ring->head] = data[written++];
//         ring->head = next;
//     }
//     return written;
// }
static size_t uartlite_ring_put(uartlite_ring_t *ring,
                                const uint8_t data)
{
    size_t next;

    if (ring == NULL)
    {
        return 0U;
    }

    next = ring->head + 1U;
    if (next >= UARTLITE_RING_BUFFER_SIZE)
    {
        next = 0U;
    }

    if (next == ring->tail)
    {
        return 0U;
    }

    ring->data[ring->head] = data;
    ring->head = next;
    return 1U;
}

static size_t uartlite_ring_available(const uartlite_ring_t *ring)
{
    if (ring->head >= ring->tail)
    {
        return ring->head - ring->tail;
    }
    return UARTLITE_RING_BUFFER_SIZE - ring->tail + ring->head;
}

// static size_t uartlite_ring_read(uartlite_ring_t *ring,
//                                  uint8_t *data,
//                                  size_t len)
// {
//     size_t read = 0U;

//     while (read < len && ring->tail != ring->head)
//     {
//         data[read++] = ring->data[ring->tail];
//         ring->tail = (ring->tail + 1U) % UARTLITE_RING_BUFFER_SIZE;
//     }
//     return read;
// }

static size_t uartlite_ring_read(uartlite_ring_t *ring, uint8_t *data, size_t max_len)
{
    size_t bytes_available, bytes_to_read, contiguous;
    //	size_t bytes_read = 0;

    if (ring == NULL)
        return 0;

    bytes_available = uartlite_ring_available(ring);

    if (bytes_available == 0)
        return 0;

    bytes_to_read = (bytes_available > max_len) ? max_len : bytes_available;

    contiguous = UARTLITE_RING_BUFFER_SIZE - ring->tail;
    if (bytes_to_read > contiguous)
    {
        memcpy(data, ring->data + ring->tail, contiguous);
        memcpy(data + contiguous, ring->data, bytes_to_read - contiguous);
        ring->tail = bytes_to_read - contiguous;
    }
    else
    {
        memcpy(data, ring->data + ring->tail, bytes_to_read);
        ring->tail = (ring->tail + bytes_to_read) % UARTLITE_RING_BUFFER_SIZE;
        // if (ring->tail >= UARTLITE_RING_BUFFER_SIZE)
        // {
        // 	ring->tail = 0;
        // }
    }

    return bytes_to_read;
}

static void uartlite_arm_receive(uartlite_device_t *device)
{
    XUartLite_Recv(&device->instance, &device->cur_rx_byte, 1);
}

static void uartlite_recv_handler(void *callback_ref, unsigned int event_data)
{
    uartlite_device_t *device = (uartlite_device_t *)callback_ref;
    BaseType_t task_woken = pdFALSE;
    size_t written;

    (void)event_data;
    /* UARTLite receive requests are one-shot; arm the next request. */
    uartlite_arm_receive(device);
    if (device == NULL || !device->opened)
    {
        return;
    }
    written = uartlite_ring_put(&device->rx_ring, device->cur_rx_byte);
    if (written != 0U && device->rx_sem != NULL)
    {
        (void)xSemaphoreGiveFromISR(device->rx_sem, &task_woken);
        // portYIELD_FROM_ISR(task_woken);
    }
}

static void uartlite_send_handler(void *callback_ref, unsigned int event_data)
{
    (void)callback_ref;
    (void)event_data;
}

static int uartlite_probe(uartlite_device_t *device)
{
    int status;

    status = XUartLite_Initialize(&device->instance, device->device_id);
    if (status != XST_SUCCESS)
    {
        return -1;
    }
    status = XUartLite_SelfTest(&device->instance);
    if (status != XST_SUCCESS)
    {
        return -1;
    }
    status = XIntc_Connect(&XIntcInstance,
                           device->interrupt_id,
                           (XInterruptHandler)XUartLite_InterruptHandler,
                           &device->instance);
    if (status != XST_SUCCESS)
    {
        return -1;
    }
    XUartLite_SetRecvHandler(&device->instance,
                             uartlite_recv_handler,
                             device);
    XUartLite_SetSendHandler(&device->instance,
                             uartlite_send_handler,
                             device);
    device->initialized = true;
    return 0;
}

int uartlite_init(void)
{
    if (uartlite_device.initialized)
    {
        return 0;
    }
    if (uartlite_device.rx_sem == NULL)
    {
        uartlite_device.rx_sem = xSemaphoreCreateBinary();
        if (uartlite_device.rx_sem == NULL)
        {
            return -1;
        }
    }
    return uartlite_probe(&uartlite_device);
}

int uartlite_open(void)
{
    if (!uartlite_device.initialized)
    {
        return -1;
    }
    uartlite_ring_flush(&uartlite_device.rx_ring);
    (void)xSemaphoreTake(uartlite_device.rx_sem, 0U);
    XUartLite_ResetFifos(&uartlite_device.instance);
    uartlite_device.opened = true;
    uartlite_arm_receive(&uartlite_device);
    XUartLite_EnableInterrupt(&uartlite_device.instance);
    XIntc_Enable(&XIntcInstance, uartlite_device.interrupt_id);
    return 0;
}

void uartlite_close(void)
{
    if (!uartlite_device.initialized)
    {
        return;
    }
    XIntc_Disable(&XIntcInstance, uartlite_device.interrupt_id);
    XUartLite_DisableInterrupt(&uartlite_device.instance);
    uartlite_device.opened = false;
    uartlite_ring_flush(&uartlite_device.rx_ring);
}

int uartlite_read_byte(TickType_t timeout, unsigned char *byte)
{
    if (byte == NULL || !uartlite_device.opened)
    {
        return -1;
    }
    if (uartlite_ring_read(&uartlite_device.rx_ring, byte, 1U) == 1U)
    {
        (void)xSemaphoreTake(uartlite_device.rx_sem, 0U);
        return 0;
    }
    if (xSemaphoreTake(uartlite_device.rx_sem, timeout) != pdTRUE)
    {
        return -1;
    }
    return uartlite_ring_read(&uartlite_device.rx_ring, byte, 1U) == 1U ? 0 : -1;
}

size_t uartlite_write_blocking(char *buffer, size_t length, TickType_t timeout)
{
    TickType_t start;
    size_t sent;

    if (buffer == NULL || length == 0U || !uartlite_device.opened)
    {
        return 0U;
    }
    start = xTaskGetTickCount();
    sent = XUartLite_Send(&uartlite_device.instance,
                          (uint8_t *)buffer,
                          (unsigned int)length);
    while (XUartLite_IsSending(&uartlite_device.instance))
    {
        if (timeout == 0U || (xTaskGetTickCount() - start) >= timeout)
        {
            break;
        }
        taskYIELD();
    }
    return sent;
}

bool uartlite_is_tx_empty(void)
{
    return !XUartLite_IsSending(&uartlite_device.instance);
}
