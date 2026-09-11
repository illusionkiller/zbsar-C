#ifndef TCP_SOCKET_PERF_SERVER_H
#define TCP_SOCKET_PERF_SERVER_H

#include "lwiperf.h"

void *tcp_socket_perf_start_server(u16_t port, lwiperf_report_fn report_fn,
                                   void *report_arg);
void tcp_socket_perf_abort(void *handle);

#endif
