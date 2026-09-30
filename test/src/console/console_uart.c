/* console_backend_ops implementations */
#include "FreeRTOS.h"
#include "uartlite.h"
#include "shell.h"
s32 console_early_read(s32 fd, char8 *buf, s32 nbytes)
{
    if (fd != 0 || buf == NULL)
    {
        return -1;
    }
    s32 i;
    s32 numbytes = 0;
    char8 *LocalBuf = buf;

    (void)fd;
    if (LocalBuf != NULL)
    {
        for (i = 0; i < nbytes; i++)
        {
            numbytes++;
            LocalBuf[i] = inbyte();
            if ((LocalBuf[i] == '\n') || (LocalBuf[i] == '\r'))
            {
                break;
            }
        }
    }

    return nbytes;
}

sint32 console_early_write(sint32 fd, char8 *buf, sint32 nbytes)
{
    if ((fd != 1 && fd != 2) || buf == NULL)
    {
        return -1;
    }
    s32 i;
    char8 *LocalBuf = buf;

    for (i = 0; i < nbytes; i++)
    {
        if (LocalBuf != NULL)
        {
            LocalBuf += i;
        }
        if (LocalBuf != NULL)
        {
            if (*LocalBuf == '\n')
            {
                while (!uartlite_is_tx_empty());
                outbyte('\r');
            }
            while (!uartlite_is_tx_empty());
            outbyte(*LocalBuf);
        }
        if (LocalBuf != NULL)
        {
            LocalBuf -= i;
        }
    }

    return nbytes;
}

static size_t uart_backend_read(console_backend_t *backend,
                                char *buf,
                                size_t len,
                                TickType_t timeout)
{
    size_t count = 0U;
    unsigned char byte;

    (void)backend;
    if (buf == NULL || len == 0U)
    {
        return 0U;
    }

    /* Wait for the first byte, then drain bytes already buffered. */
    if (uartlite_read_byte(timeout, &byte) != 0)
    {
        return 0U;
    }
    buf[count++] = (char)byte;

    while (count < len && uartlite_read_byte(0U, &byte) == 0)
    {
        buf[count++] = (char)byte;
    }

    return count;
}

static size_t uart_backend_write(console_backend_t *backend,
                                 char *buf,
                                 size_t len,
                                 TickType_t timeout)
{
    // (void)backend;
    return uartlite_write_blocking(buf, len, timeout);
    // for (size_t i = 0; i < len; i++)
    // {
        // while (!uartlite_is_tx_empty());
        // outbyte(buf[i]);
    // }
    // return len;
}

static int uart_backend_init(console_backend_t *backend)
{
    (void)backend;

    if (uartlite_init() != 0)
    {
        return -1;
    }
    return uartlite_open();
}

static void uart_backend_deinit(console_backend_t *backend)
{
    (void)backend;
    uartlite_close();
}

static const console_backend_ops_t uart_backend_ops = {
    .init = uart_backend_init,
    .deinit = uart_backend_deinit,
    .read = uart_backend_read,
    .write = uart_backend_write,
};

console_backend_t console_backend_uart = {
    .name = "uart",
    .ops = &uart_backend_ops,
    .priv = NULL,
};
