#include <lwip/sockets.h>
#include "telnet.h"

telnet_session_t telnet_session;

static telnet_event_callback_t telnet_event_callback;
static void *telnet_event_user_data;

void telnet_register_event_callback(telnet_event_callback_t callback,
                                     void *user_data)
{
    telnet_event_callback = callback;
    telnet_event_user_data = user_data;
}

static void telnet_notify_event(telnet_event_t event)
{
    if (telnet_event_callback != NULL) {
        telnet_event_callback(telnet_event_user_data, event);
    }
}

static void rb_putchar(telnet_session_t *s, uint8_t c)
{
    size_t next = (s->rx_head + 1) % TELNET_RX_BUFFER_SIZE;
    if (next == s->rx_tail) return;
    s->rx_buf[s->rx_head] = c;
    s->rx_head = next;
}

size_t telnet_rb_get(telnet_session_t *s, char *buf, size_t len)
{
    size_t count = 0;
    while (count < len && s->rx_tail != s->rx_head) {
        buf[count++] = s->rx_buf[s->rx_tail];
        s->rx_tail = (s->rx_tail + 1) % TELNET_RX_BUFFER_SIZE;
    }
    return count;
}

static void send_option(telnet_session_t *s, uint8_t option, uint8_t value)
{
    uint8_t buf[3] = { TELNET_IAC, option, value };
    if (s->client_fd >= 0)
        send(s->client_fd, buf, 3, 0);
}

static void telnet_flush_rx_sem(telnet_session_t *s)
{
    if (s->rx_sem == NULL) {
        return;
    }
    while (xSemaphoreTake(s->rx_sem, 0) == pdTRUE) {
        /* drain stale wakeups */
    }
}

static void telnet_process_rx(const uint8_t *data, size_t length)
{
    telnet_session_t *s = &telnet_session;

    for (size_t i = 0; i < length; i++) {
        uint8_t c = data[i];
        switch (s->state) {
        case STATE_IAC:
            if (c == TELNET_IAC) {
                rb_putchar(s, c);
                s->state = STATE_NORMAL;
            } else {
                switch (c) {
                case TELNET_WILL: s->state = STATE_WILL; break;
                case TELNET_WONT: s->state = STATE_WONT; break;
                case TELNET_DO:   s->state = STATE_DO;   break;
                case TELNET_DONT: s->state = STATE_DONT; break;
                default:          s->state = STATE_NORMAL; break;
                }
            }
            break;

        case STATE_WILL:
             if (c == TELNET_OPT_ECHO) {
                 /* We want the server side to echo, not the client side. */
                 send_option(s, TELNET_DONT, c);
             } else if (c == TELNET_OPT_SGA) {
                 send_option(s, TELNET_DO, c);
             } else {
                 send_option(s, TELNET_DONT, c);
             }
            s->state = STATE_NORMAL;
            break;

        case STATE_WONT:
            s->state = STATE_NORMAL;
            break;

        case STATE_DO:
             if (c == TELNET_OPT_ECHO || c == TELNET_OPT_SGA) {
                 send_option(s, TELNET_WILL, c);
             } else {
                 send_option(s, TELNET_WONT, c);
             }
            s->state = STATE_NORMAL;
            break;

        case STATE_DONT:
            s->state = STATE_NORMAL;
            break;

        case STATE_NORMAL:
            if (s->drop_cr_followup) {
                s->drop_cr_followup = 0;
                if (c == '\n' || c == '\0') {
                    break;
                }
            }

            if (c == TELNET_IAC) {
                s->state = STATE_IAC;
            }
            else if (c == '\r') {
                rb_putchar(s, '\r');
                xSemaphoreGive(s->rx_sem);
                s->drop_cr_followup = 1;
            }
            else if (c == '\n') {
                rb_putchar(s, '\r');
                xSemaphoreGive(s->rx_sem);
            }
            else {
                rb_putchar(s, c);
                xSemaphoreGive(s->rx_sem);
            }
            break;
        }
    }
}

static void telnet_session_set_fd(int fd)
{
    telnet_session.client_fd = fd;
    telnet_session.rx_head = 0;
    telnet_session.rx_tail = 0;
    telnet_session.state = STATE_NORMAL;
    telnet_session.drop_cr_followup = 0;
    telnet_flush_rx_sem(&telnet_session);
}

static void telnet_session_close(void)
{
    telnet_session.client_fd = -1;
    telnet_session.rx_head = 0;
    telnet_session.rx_tail = 0;
    telnet_session.state = STATE_NORMAL;
    telnet_session.drop_cr_followup = 0;
    if (telnet_session.rx_sem)
    {
        xSemaphoreGive(telnet_session.rx_sem);
    }
}

static void telnet_send_negotiate(void)
{
    send_option(&telnet_session, TELNET_WILL, TELNET_OPT_ECHO);
    send_option(&telnet_session, TELNET_WILL, TELNET_OPT_SGA);
    send_option(&telnet_session, TELNET_DO, TELNET_OPT_SGA);
}

static void app_telnet_server_task(void *arg)
{
    struct sockaddr_in addr;
    socklen_t addr_size;
    int server_fd;
    int client_fd;
    int recv_len;
    uint8_t recv_buf[TELNET_RECV_BUF_SIZE];

    (void)arg;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("telnet: socket failed\r\n");
        vTaskDelete(NULL);
        return;
    }

    {
        int reuse = 1;
        (void)setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TELNET_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("telnet: bind failed\r\n");
        closesocket(server_fd);
        vTaskDelete(NULL);
        return;
    }

    if (listen(server_fd, 1) < 0) {
        printf("telnet: listen failed\r\n");
        closesocket(server_fd);
        vTaskDelete(NULL);
        return;
    }

    printf("telnet: server listening on port %d\r\n", TELNET_PORT);

    while (1) {
        addr_size = sizeof(addr);
        client_fd = accept(server_fd, (struct sockaddr *)&addr, &addr_size);
        if (client_fd < 0) {
            continue;
        }

        printf("telnet: client connected\r\n");
        telnet_session_set_fd(client_fd);
        telnet_send_negotiate();
        telnet_notify_event(TELNET_EVENT_CONNECTED);
        printf("Welcome to the Xilinx Shell!\r\n");

        while (1) {
            recv_len = recv(client_fd, recv_buf, sizeof(recv_buf), 0);
            if (recv_len <= 0) {
                break;
            }
            telnet_process_rx(recv_buf, (size_t)recv_len);
        }

        telnet_notify_event(TELNET_EVENT_DISCONNECTED);
        telnet_session_close();
        closesocket(client_fd);
        printf("telnet: client disconnected\r\n");
    }
}

int telnet_service_start(void)
{
    return xTaskCreate(app_telnet_server_task,
                       "telnet_srv",
                       TELNET_SERVER_STACK,
                       NULL,
                       tskIDLE_PRIORITY + 2,
                       NULL) == pdTRUE ? 0 : -1;
}
