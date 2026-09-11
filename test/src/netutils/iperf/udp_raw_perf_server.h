#ifndef UDP_RAW_PERF_SERVER_H
#define UDP_RAW_PERF_SERVER_H

#include "udp_perf_server.h"

void *udp_raw_perf_start_server(u16_t port, udp_perf_report_fn report_fn,
                                void *report_arg);
void *udp_raw_perf_start_client(const ip_addr_t *remote_addr, u16_t port,
                                u32_t bandwidth_bps, u32_t duration_sec,
                                udp_perf_report_fn report_fn,
                                void *report_arg);
void udp_raw_perf_abort(void *handle);

#endif
