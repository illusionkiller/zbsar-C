#include "tcp_socket_perf_server.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/api.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"

#include <string.h>

#define TCP_NETCONN_TASK_STACK_SIZE 2048
#define TCP_NETCONN_RECV_UPDATE_SIZE (64 * 1024)

struct tcp_socket_perf_session {
    struct netconn *listen_conn;
    struct netconn *active_conn;
    volatile int stop;
    TaskHandle_t task;
    SemaphoreHandle_t done;
    lwiperf_report_fn report_fn;
    void *report_arg;
};

static void tcp_netconn_report(struct tcp_socket_perf_session *session,
                               const ip_addr_t *local_addr, u16_t local_port,
                               const ip_addr_t *remote_addr, u16_t remote_port,
                               enum lwiperf_report_type report_type,
                               u32_t bytes, u32_t duration_ms)
{
    u32_t bandwidth = 0;

    if (duration_ms != 0) {
        bandwidth = (u32_t)(((u64_t)bytes * 8ULL) / duration_ms);
    }
    if (session->report_fn != NULL) {
        session->report_fn(session->report_arg, report_type,
                           local_addr, local_port,
                           remote_addr, remote_port,
                           bytes, duration_ms, bandwidth);
    }
}

static void tcp_netconn_delete(struct netconn **conn_ptr)
{
    struct netconn *conn;

    if ((conn_ptr == NULL) || (*conn_ptr == NULL)) {
        return;
    }
    conn = *conn_ptr;
    *conn_ptr = NULL;
    (void)netconn_close(conn);
    (void)netconn_delete(conn);
}

static void tcp_netconn_task(void *arg)
{
    struct tcp_socket_perf_session *session =
        (struct tcp_socket_perf_session *)arg;
    struct netconn *conn;
    struct pbuf *p;
    ip_addr_t local_addr;
    ip_addr_t remote_addr;
    u16_t local_port = 0;
    u16_t remote_port = 0;

    while (!session->stop) {
        err_t err;
        u32_t bytes = 0;
        u32_t pending_window_update = 0;
        u32_t start_ms;

        conn = NULL;
        err = netconn_accept(session->listen_conn, &conn);
        if ((err != ERR_OK) || (conn == NULL)) {
            if (!session->stop) {
                vTaskDelay(pdMS_TO_TICKS(1));
            }
            continue;
        }

        session->active_conn = conn;
        ip_addr_set_zero(&local_addr);
        ip_addr_set_zero(&remote_addr);
        (void)netconn_addr(conn, &local_addr, &local_port);
        (void)netconn_peer(conn, &remote_addr, &remote_port);
        start_ms = sys_now();

        while (!session->stop) {
            p = NULL;
            err = netconn_recv_tcp_pbuf_flags(conn, &p, NETCONN_NOAUTORCVD);
            if (err != ERR_OK) {
                break;
            }
            if (p == NULL) {
                break;
            }

            bytes += p->tot_len;
            pending_window_update += p->tot_len;
            pbuf_free(p);

            /* Batch TCP window updates while retaining zero-copy reception. */
            if (pending_window_update >= TCP_NETCONN_RECV_UPDATE_SIZE) {
                (void)netconn_tcp_recvd(conn, pending_window_update);
                pending_window_update = 0;
            }
        }

        if (pending_window_update != 0) {
            (void)netconn_tcp_recvd(conn, pending_window_update);
        }

        tcp_netconn_report(session, &local_addr, local_port,
                           &remote_addr, remote_port,
                           session->stop ? LWIPERF_TCP_ABORTED_LOCAL :
                                            LWIPERF_TCP_DONE_SERVER,
                           bytes, sys_now() - start_ms);
        tcp_netconn_delete(&conn);
        session->active_conn = NULL;
    }

    tcp_netconn_delete(&session->active_conn);
    tcp_netconn_delete(&session->listen_conn);
    session->task = NULL;
    xSemaphoreGive(session->done);
    vTaskDelete(NULL);
}

void *tcp_socket_perf_start_server(u16_t port, lwiperf_report_fn report_fn,
                                   void *report_arg)
{
    struct tcp_socket_perf_session *session;
    err_t err;

    session = (struct tcp_socket_perf_session *)mem_malloc(sizeof(*session));
    if (session == NULL) {
        return NULL;
    }
    memset(session, 0, sizeof(*session));
    session->report_fn = report_fn;
    session->report_arg = report_arg;
    session->done = xSemaphoreCreateBinary();
    if (session->done == NULL) {
        mem_free(session);
        return NULL;
    }

    session->listen_conn = netconn_new(NETCONN_TCP);
    if (session->listen_conn == NULL) {
        vSemaphoreDelete(session->done);
        mem_free(session);
        return NULL;
    }
    err = netconn_bind(session->listen_conn, IP_ADDR_ANY, port);
    if (err == ERR_OK) {
        err = netconn_listen_with_backlog(session->listen_conn, 1);
    }
    if (err != ERR_OK) {
        tcp_netconn_delete(&session->listen_conn);
        vSemaphoreDelete(session->done);
        mem_free(session);
        return NULL;
    }

    if (xTaskCreate(tcp_netconn_task, "iperf_tcp_netconn",
                    TCP_NETCONN_TASK_STACK_SIZE, session,
                    DEFAULT_THREAD_PRIO, &session->task) != pdTRUE) {
        tcp_netconn_delete(&session->listen_conn);
        vSemaphoreDelete(session->done);
        mem_free(session);
        return NULL;
    }
    return session;
}

void tcp_socket_perf_abort(void *handle)
{
    struct tcp_socket_perf_session *session =
        (struct tcp_socket_perf_session *)handle;

    if (session == NULL) {
        return;
    }
    session->stop = 1;
    if (session->active_conn != NULL) {
        (void)netconn_close(session->active_conn);
    }
    if (session->listen_conn != NULL) {
        (void)netconn_close(session->listen_conn);
    }
    if (session->task != NULL && session->task != xTaskGetCurrentTaskHandle()) {
        (void)xSemaphoreTake(session->done, pdMS_TO_TICKS(1000));
    }
    vSemaphoreDelete(session->done);
    mem_free(session);
}
