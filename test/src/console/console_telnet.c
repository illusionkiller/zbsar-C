/* console_backend_ops implementations */
#include <lwip/sockets.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "telnet.h"
#include "shell.h"
static size_t telnet_backend_read(console_backend_t *backend, char *buf, size_t len, TickType_t timeout)
{
    telnet_session_t *s = (telnet_session_t *)backend->priv;
    size_t count;

    if (s->client_fd < 0 || len == 0)
    {
        return 0;
    }

    count = telnet_rb_get(s, buf, len);
    if (count > 0 || timeout == 0)
    {
        return count;
    }

    if (xSemaphoreTake(s->rx_sem, timeout) != pdTRUE || s->client_fd < 0)
    {
        return 0;
    }

    return telnet_rb_get(s, buf, len);
}

static size_t telnet_backend_write(console_backend_t *backend, char *buf, size_t len, TickType_t timeout)
{
    telnet_session_t *s = (telnet_session_t *)backend->priv;
    (void)timeout;
    if (s->client_fd < 0)
        return 0;
    int ret = send(s->client_fd, buf, len, 0);
    return (ret > 0) ? (size_t)ret : 0;
}

static int telnet_backend_init(console_backend_t *backend)
{
    telnet_session_t *s = (telnet_session_t *)backend->priv;
    s->client_fd = -1;
    s->rx_head = 0;
    s->rx_tail = 0;
    s->state = STATE_NORMAL;
    s->drop_cr_followup = 0;
    s->rx_sem = xSemaphoreCreateBinary();
    return (s->rx_sem != NULL) ? 0 : -1;
}

static void telnet_backend_deinit(console_backend_t *backend)
{
    telnet_session_t *s = (telnet_session_t *)backend->priv;
    if (s->rx_sem)
    {
        vSemaphoreDelete(s->rx_sem);
        s->rx_sem = NULL;
    }
}

static const console_backend_ops_t telnet_backend_ops = {
    .init = telnet_backend_init,
    .deinit = telnet_backend_deinit,
    .read = telnet_backend_read,
    .write = telnet_backend_write,
};

console_backend_t console_backend_telnet = {
    .name = "telnet",
    .ops = &telnet_backend_ops,
    .priv = &telnet_session,
};
