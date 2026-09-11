#ifndef UDP_PERF_SERVER_H
#define UDP_PERF_SERVER_H

#include "lwip/ip_addr.h"
#include "lwip/inet.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UDP_PERF_PACKET_SIZE 1470
#define UDP_PERF_TASK_STACK_SIZE 2048

enum udp_perf_report_type {
    UDP_PERF_DONE_SERVER,
    UDP_PERF_DONE_CLIENT,
    UDP_PERF_ABORTED
};

typedef void (*udp_perf_report_fn)(
    void *arg, enum udp_perf_report_type report_type,
    const ip_addr_t *remote_addr, u16_t remote_port,
    u64_t bytes, u64_t packets, u64_t lost,
    u32_t duration_ms, u32_t bandwidth_kbitpsec);

void *udp_perf_start_server(u16_t port, udp_perf_report_fn report_fn,
                            void *report_arg);
void *udp_perf_start_client(const ip_addr_t *remote_addr, u16_t port,
                            u32_t bandwidth_bps, u32_t duration_sec,
                            udp_perf_report_fn report_fn, void *report_arg);
void udp_perf_abort(void *handle);

#ifdef __cplusplus
}
#endif

#endif
