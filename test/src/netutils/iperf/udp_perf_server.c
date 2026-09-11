#include "udp_perf_server.h"
#include "udp_iperf2_protocol.h"

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "lwip/mem.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

struct udp_perf_session {
    int sock;
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
};

static u32_t udp_elapsed_ms(u32_t start)
{
    return sys_now() - start;
}

static u32_t udp_bandwidth_kbps(u64_t bytes, u32_t duration_ms)
{
    u64_t value;

    if (duration_ms == 0) {
        return 0;
    }

    value = (bytes * 8ULL) / duration_ms;
    if (value > 0xffffffffULL) {
        return 0xffffffffU;
    }
    return (u32_t)value;
}

static void udp_report(struct udp_perf_session *session,
                       enum udp_perf_report_type report_type,
                       u64_t bytes, u64_t packets, u64_t lost,
                       u32_t duration_ms)
{
    if (session->report_fn != NULL) {
        session->report_fn(session->report_arg, report_type,
                           &session->remote_addr,
                           session->remote_port, bytes, packets, lost,
                           duration_ms, udp_bandwidth_kbps(bytes, duration_ms));
    }
}

static void udp_wait_for_rate(struct udp_perf_session *session,
                              u64_t bytes_sent, u32_t start_ms)
{
    u64_t target_ms;
    u32_t elapsed_ms;
    u32_t wait_ms;

    if (session->bandwidth_bps == 0) {
        return;
    }

    target_ms = (bytes_sent * 8000ULL) / session->bandwidth_bps;
    elapsed_ms = udp_elapsed_ms(start_ms);
    if (target_ms <= elapsed_ms) {
        return;
    }

    wait_ms = (u32_t)(target_ms - elapsed_ms);
    if (wait_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

static void udp_perf_client_task(void *arg)
{
    struct udp_perf_session *session = (struct udp_perf_session *)arg;
    struct sockaddr_in remote;
    u8_t packet[UDP_PERF_PACKET_SIZE];
    u32_t start_ms;
    u32_t duration_ms;
    u64_t bytes = 0;
    u64_t packets = 0;
    int32_t sequence = 0;
    int sent;

    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(session->remote_port);
    remote.sin_addr.s_addr = ip4_addr_get_u32(ip_2_ip4(&session->remote_addr));

    memset(packet, 'A', sizeof(packet));
    start_ms = sys_now();
    duration_ms = session->duration_sec * 1000U;

    while (!session->stop && udp_elapsed_ms(start_ms) < duration_ms) {
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);
        udp_iperf2_encode_header(packet, sequence, &timestamp);

        sent = lwip_sendto(session->sock, packet, sizeof(packet), 0,
                           (struct sockaddr *)&remote, sizeof(remote));
        if (sent > 0) {
            bytes += (u32_t)sent;
            packets++;
            sequence++;
            udp_wait_for_rate(session, bytes, start_ms);
        } else if (session->stop) {
            break;
        } else {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    if (!session->stop) {
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);
        udp_iperf2_encode_header(packet, UDP_IPERF2_END_SEQUENCE, &timestamp);
        (void)lwip_sendto(session->sock, packet, sizeof(packet), 0,
                          (struct sockaddr *)&remote, sizeof(remote));
        udp_report(session, UDP_PERF_DONE_CLIENT, bytes, packets, 0,
                   udp_elapsed_ms(start_ms));
    } else {
        udp_report(session, UDP_PERF_ABORTED, bytes, packets, 0,
                   udp_elapsed_ms(start_ms));
    }

    lwip_close(session->sock);
    session->sock = -1;
    session->task = NULL;
    xSemaphoreGive(session->done);
    vTaskDelete(NULL);
}

static void udp_perf_server_task(void *arg)
{
    struct udp_perf_session *session = (struct udp_perf_session *)arg;
    struct sockaddr_in remote;
    socklen_t remote_len;
    struct timeval timeout;
    u8_t packet[UDP_PERF_PACKET_SIZE];
    u32_t start_ms = 0;
    u64_t bytes = 0;
    u64_t packets = 0;
    u64_t lost = 0;
    int32_t expected_sequence = 0;
    int first_packet = 1;
    int received;

    timeout.tv_sec = 0;
    timeout.tv_usec = 200000;
    (void)lwip_setsockopt(session->sock, SOL_SOCKET, SO_RCVTIMEO,
                          &timeout, sizeof(timeout));

    while (!session->stop) {
        remote_len = sizeof(remote);
        received = lwip_recvfrom(session->sock, packet, sizeof(packet), 0,
                                 (struct sockaddr *)&remote, &remote_len);
        if (received < (int)UDP_IPERF2_HEADER_SIZE) {
            continue;
        }

        {
            int32_t sequence = udp_iperf2_decode_sequence(packet);

            if (sequence < 0) {
                (void)lwip_sendto(session->sock, packet, received, 0,
                                  (struct sockaddr *)&remote, remote_len);
                if (!first_packet) {
                    udp_report(session, UDP_PERF_DONE_SERVER, bytes, packets,
                               lost, udp_elapsed_ms(start_ms));
                }
                first_packet = 1;
                bytes = 0;
                packets = 0;
                lost = 0;
                expected_sequence = 0;
                continue;
            }

            if (first_packet) {
                first_packet = 0;
                start_ms = sys_now();
                expected_sequence = sequence;
                ip_addr_set_ip4_u32(&session->remote_addr,
                                    remote.sin_addr.s_addr);
                session->remote_port = ntohs(remote.sin_port);
            }

            if (sequence > expected_sequence) {
                lost += (u32_t)(sequence - expected_sequence);
            } else if (sequence < expected_sequence) {
                lost++;
            }
            expected_sequence = sequence + 1;
            bytes += (u32_t)received;
            packets++;
        }
    }

    if (!first_packet) {
        udp_report(session, UDP_PERF_ABORTED, bytes, packets, lost,
                   udp_elapsed_ms(start_ms));
    }

    lwip_close(session->sock);
    session->sock = -1;
    session->task = NULL;
    xSemaphoreGive(session->done);
    vTaskDelete(NULL);
}

static void *udp_perf_start_common(int server, const ip_addr_t *remote_addr,
                                   u16_t port, u32_t bandwidth_bps,
                                   u32_t duration_sec,
                                   udp_perf_report_fn report_fn,
                                   void *report_arg)
{
    struct udp_perf_session *session;
    struct sockaddr_in local;
    int reuse = 1;

    session = (struct udp_perf_session *)mem_malloc(sizeof(*session));
    if (session == NULL) {
        return NULL;
    }
    memset(session, 0, sizeof(*session));
    session->sock = -1;
    session->server = server;
    session->port = port;
    session->duration_sec = duration_sec;
    session->bandwidth_bps = bandwidth_bps;
    session->report_fn = report_fn;
    session->report_arg = report_arg;
    if (remote_addr != NULL) {
        ip_addr_copy(session->remote_addr, *remote_addr);
        session->remote_port = port;
    }

    session->done = xSemaphoreCreateBinary();
    if (session->done == NULL) {
        mem_free(session);
        return NULL;
    }

    session->sock = lwip_socket(AF_INET, SOCK_DGRAM, 0);
    if (session->sock < 0) {
        vSemaphoreDelete(session->done);
        mem_free(session);
        return NULL;
    }

    (void)lwip_setsockopt(session->sock, SOL_SOCKET, SO_REUSEADDR,
                          &reuse, sizeof(reuse));

    if (server) {
        memset(&local, 0, sizeof(local));
        local.sin_family = AF_INET;
        local.sin_port = htons(port);
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        if (lwip_bind(session->sock, (struct sockaddr *)&local,
                      sizeof(local)) < 0) {
            lwip_close(session->sock);
            vSemaphoreDelete(session->done);
            mem_free(session);
            return NULL;
        }
    }

    if (xTaskCreate(server ? udp_perf_server_task : udp_perf_client_task,
                    server ? "iperf_udp_srv" : "iperf_udp_cli",
                    UDP_PERF_TASK_STACK_SIZE, session, DEFAULT_THREAD_PRIO,
                    &session->task) != pdTRUE) {
        lwip_close(session->sock);
        vSemaphoreDelete(session->done);
        mem_free(session);
        return NULL;
    }
    return session;
}

void *udp_perf_start_server(u16_t port, udp_perf_report_fn report_fn,
                            void *report_arg)
{
    return udp_perf_start_common(1, NULL, port, 0, 0, report_fn, report_arg);
}

void *udp_perf_start_client(const ip_addr_t *remote_addr, u16_t port,
                            u32_t bandwidth_bps, u32_t duration_sec,
                            udp_perf_report_fn report_fn, void *report_arg)
{
    if (remote_addr == NULL || duration_sec == 0) {
        return NULL;
    }
    return udp_perf_start_common(0, remote_addr, port, bandwidth_bps,
                                 duration_sec, report_fn, report_arg);
}

void udp_perf_abort(void *handle)
{
    struct udp_perf_session *session = (struct udp_perf_session *)handle;

    if (session == NULL) {
        return;
    }

    session->stop = 1;
    if (session->sock >= 0) {
        lwip_close(session->sock);
    }
    if (session->task != NULL &&
        session->task != xTaskGetCurrentTaskHandle()) {
        (void)xSemaphoreTake(session->done, pdMS_TO_TICKS(1000));
    }
    vSemaphoreDelete(session->done);
    mem_free(session);
}
