#include "udp_raw_perf_server.h"
#include "udp_iperf2_protocol.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"
#include "lwip/ip_addr.h"

#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#define UDP_RAW_RATE_SLICE_MS 10

struct udp_raw_perf_session {
    struct udp_pcb *pcb;
    int server;
    volatile int stop;
    TaskHandle_t task;
    SemaphoreHandle_t done;
    u16_t port;
    u32_t duration_sec;
    u32_t bandwidth_bps;
    udp_perf_report_fn report_fn;
    void *report_arg;
    ip_addr_t remote_addr;
    u16_t remote_port;
    u32_t start_ms;
    u64_t bytes;
    u64_t packets;
    u64_t lost;
    s32_t expected_sequence;
    int first_packet;
};

static u32_t udp_raw_bandwidth_kbps(u64_t bytes, u32_t duration_ms)
{
    u64_t value;

    if (duration_ms == 0) {
        return 0;
    }
    value = (bytes * 8ULL) / duration_ms;
    return (value > 0xffffffffULL) ? 0xffffffffU : (u32_t)value;
}

static void udp_raw_report(struct udp_raw_perf_session *session,
                            enum udp_perf_report_type report_type)
{
    if (session->report_fn != NULL) {
        u32_t duration_ms = sys_now() - session->start_ms;
        session->report_fn(session->report_arg, report_type,
                           &session->remote_addr, session->remote_port,
                           session->bytes, session->packets, session->lost,
                           duration_ms,
                           udp_raw_bandwidth_kbps(session->bytes,
                                                  duration_ms));
    }
}

static int udp_raw_read_sequence(const struct pbuf *p, s32_t *sequence)
{
    s32_t network_sequence;

    if ((p == NULL) || (sequence == NULL) || (p->tot_len < sizeof(network_sequence))) {
        return 0;
    }
    if (pbuf_copy_partial(p, &network_sequence, sizeof(network_sequence), 0) !=
        sizeof(network_sequence)) {
        return 0;
    }
    *sequence = (s32_t)lwip_ntohl((u32_t)network_sequence);
    return 1;
}

static void udp_raw_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
                         const ip_addr_t *addr, u16_t port)
{
    struct udp_raw_perf_session *session = (struct udp_raw_perf_session *)arg;
    s32_t sequence;

    LWIP_UNUSED_ARG(pcb);
    if ((session == NULL) || (p == NULL)) {
        if (p != NULL) {
            pbuf_free(p);
        }
        return;
    }

    if (addr != NULL) {
        ip_addr_copy(session->remote_addr, *addr);
        session->remote_port = port;
    }

    if (!udp_raw_read_sequence(p, &sequence)) {
        pbuf_free(p);
        return;
    }

    if (sequence < 0) {
        if (!session->first_packet) {
            udp_raw_report(session, UDP_PERF_DONE_SERVER);
        }
        session->first_packet = 1;
        session->bytes = 0;
        session->packets = 0;
        session->lost = 0;
        session->expected_sequence = 0;
        pbuf_free(p);
        return;
    }

    if (session->first_packet) {
        session->first_packet = 0;
        session->start_ms = sys_now();
        session->expected_sequence = sequence;
    }

    if (sequence > session->expected_sequence) {
        session->lost += (u32_t)(sequence - session->expected_sequence);
    } else if (sequence < session->expected_sequence) {
        session->lost++;
    }
    session->expected_sequence = sequence + 1;
    session->bytes += p->tot_len;
    session->packets++;
    pbuf_free(p);
}

static void udp_raw_remove_pcb(struct udp_raw_perf_session *session)
{
    struct udp_pcb *pcb;

    LOCK_TCPIP_CORE();
    pcb = session->pcb;
    session->pcb = NULL;
    if (pcb != NULL) {
        udp_recv(pcb, NULL, NULL);
        udp_remove(pcb);
    }
    UNLOCK_TCPIP_CORE();
}

static void udp_raw_wait_for_rate(struct udp_raw_perf_session *session,
                                  u64_t bytes_sent)
{
    u64_t target_ms;
    u32_t elapsed_ms;
    u32_t wait_ms;
    u32_t slice_ms;

    if (session->bandwidth_bps == 0) {
        return;
    }
    target_ms = (bytes_sent * 8000ULL) / session->bandwidth_bps;
    elapsed_ms = sys_now() - session->start_ms;
    if (target_ms <= elapsed_ms) {
        return;
    }
    wait_ms = (u32_t)(target_ms - elapsed_ms);
    while (!session->stop && wait_ms > 0) {
        slice_ms = (wait_ms > UDP_RAW_RATE_SLICE_MS) ?
                   UDP_RAW_RATE_SLICE_MS : wait_ms;
        vTaskDelay(pdMS_TO_TICKS(slice_ms));
        elapsed_ms = sys_now() - session->start_ms;
        if (target_ms <= elapsed_ms) {
            break;
        }
        wait_ms = (u32_t)(target_ms - elapsed_ms);
    }
}

static void udp_raw_client_task(void *arg)
{
    struct udp_raw_perf_session *session = (struct udp_raw_perf_session *)arg;
    struct pbuf *p = NULL;
    u8_t packet[UDP_PERF_PACKET_SIZE];
    u32_t start_ms;
    u64_t bytes = 0;
    u64_t packets = 0;
    s32_t sequence = 0;

    memset(packet, 'A', sizeof(packet));
    p = pbuf_alloc(PBUF_TRANSPORT, sizeof(packet), PBUF_RAM);
    if (p == NULL) {
        session->task = NULL;
        xSemaphoreGive(session->done);
        vTaskDelete(NULL);
        return;
    }

    start_ms = sys_now();
    session->start_ms = start_ms;
    while (!session->stop && (sys_now() - start_ms) < session->duration_sec * 1000U) {
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);
        udp_iperf2_encode_header(packet, sequence, &timestamp);
        memcpy(p->payload, packet, sizeof(packet));

        LOCK_TCPIP_CORE();
        if ((session->pcb == NULL) || (udp_send(session->pcb, p) != ERR_OK)) {
            session->stop = 1;
        }
        UNLOCK_TCPIP_CORE();

        if (!session->stop) {
            bytes += sizeof(packet);
            packets++;
            sequence++;
            udp_raw_wait_for_rate(session, bytes);
        }
    }

    if (!session->stop) {
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);
        udp_iperf2_encode_header(packet, UDP_IPERF2_END_SEQUENCE, &timestamp);
        memcpy(p->payload, packet, sizeof(packet));
        LOCK_TCPIP_CORE();
        if (session->pcb != NULL) {
            (void)udp_send(session->pcb, p);
        }
        UNLOCK_TCPIP_CORE();
        session->bytes = bytes;
        session->packets = packets;
        udp_raw_report(session, UDP_PERF_DONE_CLIENT);
    } else {
        session->bytes = bytes;
        session->packets = packets;
        udp_raw_report(session, UDP_PERF_ABORTED);
    }

    pbuf_free(p);
    udp_raw_remove_pcb(session);
    session->task = NULL;
    xSemaphoreGive(session->done);
    vTaskDelete(NULL);
}

static void *udp_raw_start_common(int server, const ip_addr_t *remote_addr,
                                  u16_t port, u32_t bandwidth_bps,
                                  u32_t duration_sec,
                                  udp_perf_report_fn report_fn,
                                  void *report_arg)
{
    struct udp_raw_perf_session *session;
    err_t err;

    session = (struct udp_raw_perf_session *)mem_malloc(sizeof(*session));
    if (session == NULL) {
        return NULL;
    }
    memset(session, 0, sizeof(*session));
    session->server = server;
    session->port = port;
    session->duration_sec = duration_sec;
    session->bandwidth_bps = bandwidth_bps;
    session->report_fn = report_fn;
    session->report_arg = report_arg;
    session->first_packet = 1;
    if (remote_addr != NULL) {
        ip_addr_copy(session->remote_addr, *remote_addr);
        session->remote_port = port;
    }

    session->pcb = udp_new_ip_type(IPADDR_TYPE_V4);
    if (session->pcb == NULL) {
        mem_free(session);
        return NULL;
    }

    if (server) {
        err = udp_bind(session->pcb, IP_ADDR_ANY, port);
        if (err != ERR_OK) {
            udp_remove(session->pcb);
            mem_free(session);
            return NULL;
        }
        udp_recv(session->pcb, udp_raw_recv, session);
        return session;
    }

    session->done = xSemaphoreCreateBinary();
    if (session->done == NULL) {
        udp_remove(session->pcb);
        mem_free(session);
        return NULL;
    }
    err = udp_connect(session->pcb, remote_addr, port);
    if (err != ERR_OK || xTaskCreate(udp_raw_client_task, "iperf_udp_raw",
                                      UDP_PERF_TASK_STACK_SIZE, session,
                                      DEFAULT_THREAD_PRIO, &session->task) != pdTRUE) {
        if (err == ERR_OK) {
            udp_disconnect(session->pcb);
        }
        udp_remove(session->pcb);
        vSemaphoreDelete(session->done);
        mem_free(session);
        return NULL;
    }
    return session;
}

void *udp_raw_perf_start_server(u16_t port, udp_perf_report_fn report_fn,
                                void *report_arg)
{
    return udp_raw_start_common(1, NULL, port, 0, 0, report_fn, report_arg);
}

void *udp_raw_perf_start_client(const ip_addr_t *remote_addr, u16_t port,
                                u32_t bandwidth_bps, u32_t duration_sec,
                                udp_perf_report_fn report_fn, void *report_arg)
{
    if ((remote_addr == NULL) || (duration_sec == 0)) {
        return NULL;
    }
    return udp_raw_start_common(0, remote_addr, port, bandwidth_bps,
                                duration_sec, report_fn, report_arg);
}

void udp_raw_perf_abort(void *handle)
{
    struct udp_raw_perf_session *session = (struct udp_raw_perf_session *)handle;

    if (session == NULL) {
        return;
    }
    session->stop = 1;
    if (session->server) {
        udp_raw_remove_pcb(session);
    } else {
        udp_raw_remove_pcb(session);
        if (session->task != NULL && session->task != xTaskGetCurrentTaskHandle()) {
            (void)xSemaphoreTake(session->done, pdMS_TO_TICKS(1000));
        }
        vSemaphoreDelete(session->done);
    }
    mem_free(session);
}
