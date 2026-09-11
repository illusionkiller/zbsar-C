#include "lwiperf.h"
#include "udp_perf_server.h"
#include "udp_raw_perf_server.h"
#include "tcp_socket_perf_server.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"
#include "lwip/tcpip.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "shell.h"

// 全局变量用于存储当前运行的iperf会话
static void* g_current_iperf_session = NULL;
enum iperf_session_kind {
    IPERF_SESSION_NONE = 0,
    IPERF_SESSION_TCP_RAW,
    IPERF_SESSION_TCP_SOCKET,
    IPERF_SESSION_UDP_SOCKET,
    IPERF_SESSION_UDP_RAW
};
static enum iperf_session_kind g_current_iperf_kind = IPERF_SESSION_NONE;

static int iperf_parse_bandwidth(const char *text, u32_t *bandwidth_bps)
{
    char *end;
    unsigned long value;
    u64_t scaled;

    if (text == NULL || bandwidth_bps == NULL || text[0] == '\0') {
        return 0;
    }

    value = strtoul(text, &end, 10);
    if (end == text) {
        return 0;
    }

    scaled = value;
    if (*end == 'k' || *end == 'K') {
        scaled *= 1000ULL;
        end++;
    } else if (*end == 'm' || *end == 'M') {
        scaled *= 1000000ULL;
        end++;
    } else if (*end == 'g' || *end == 'G') {
        scaled *= 1000000000ULL;
        end++;
    }

    if (*end != '\0' || scaled == 0 || scaled > 0xffffffffULL) {
        return 0;
    }

    *bandwidth_bps = (u32_t)scaled;
    return 1;
}

// 报告回调函数
static void iperf_report(void *arg, enum lwiperf_report_type report_type,
                         const ip_addr_t* local_addr, u16_t local_port,
                         const ip_addr_t* remote_addr, u16_t remote_port,
						 u32_t bytes_transferred, u32_t ms_duration, u32_t bandwidth_kbitpsec)
{
    char local_ip_str[16];
    char remote_ip_str[16];

    ipaddr_ntoa_r(local_addr, local_ip_str, sizeof(local_ip_str));
    ipaddr_ntoa_r(remote_addr, remote_ip_str, sizeof(remote_ip_str));

    printf("\n[Iperf Report]\n");
    printf("Local: %s:%d\n", local_ip_str, local_port);
    printf("Remote: %s:%d\n", remote_ip_str, remote_port);
    printf("Bytes transferred: %u\n", bytes_transferred);
    printf("Duration: %u ms\n", ms_duration);
    printf("Bandwidth: %u kbit/s\n", bandwidth_kbitpsec);

    switch(report_type) {
        case LWIPERF_TCP_DONE_SERVER:
            printf("Status: Server test completed\n");
            break;
        case LWIPERF_TCP_DONE_CLIENT:
            printf("Status: Client test completed\n");
            break;
        case LWIPERF_TCP_ABORTED_LOCAL:
            printf("Status: Test aborted (local error)\n");
            break;
        case LWIPERF_TCP_ABORTED_LOCAL_DATAERROR:
            printf("Status: Test aborted (data error)\n");
            break;
        case LWIPERF_TCP_ABORTED_LOCAL_TXERROR:
            printf("Status: Test aborted (transmission error)\n");
            break;
        case LWIPERF_TCP_ABORTED_REMOTE:
            printf("Status: Test aborted (remote)\n");
            break;
        default:
            printf("Status: Unknown\n");
            break;
    }

    // 重置当前会话
    /* Keep the master handle: a server listener remains active after a
       completed test and can still be stopped with `iperf -k`. */
}

// 停止正在运行的iperf会话（内部函数）
static void iperf_udp_report(void *arg,
                             enum udp_perf_report_type report_type,
                             const ip_addr_t *remote_addr, u16_t remote_port,
                             u64_t bytes, u64_t packets, u64_t lost,
                             u32_t duration_ms, u32_t bandwidth_kbitpsec)
{
    char remote_ip_str[16] = "unknown";

    if (remote_addr != NULL) {
        ipaddr_ntoa_r(remote_addr, remote_ip_str, sizeof(remote_ip_str));
    }

    printf("\n[UDP Iperf Report]\n");
    printf("Remote: %s:%d\n", remote_ip_str, remote_port);
    printf("Bytes transferred: %llu\n", bytes);
    printf("Datagrams: %llu\n", packets);
    printf("Lost/out-of-order: %llu\n", lost);
    printf("Duration: %u ms\n", duration_ms);
    printf("Bandwidth: %u kbit/s\n", bandwidth_kbitpsec);

    switch (report_type) {
        case UDP_PERF_DONE_SERVER:
            printf("Status: UDP server test completed\n");
            break;
        case UDP_PERF_DONE_CLIENT:
            printf("Status: UDP client test completed\n");
            break;
        default:
            printf("Status: UDP test aborted\n");
            break;
    }
}

static void iperf_stop_session(void)
{
    if (g_current_iperf_session != NULL) {
        switch (g_current_iperf_kind) {
        case IPERF_SESSION_UDP_SOCKET:
            udp_perf_abort(g_current_iperf_session);
            break;
        case IPERF_SESSION_UDP_RAW:
            udp_raw_perf_abort(g_current_iperf_session);
            break;
        case IPERF_SESSION_TCP_SOCKET:
            tcp_socket_perf_abort(g_current_iperf_session);
            break;
        case IPERF_SESSION_TCP_RAW:
            LOCK_TCPIP_CORE();
            lwiperf_abort(g_current_iperf_session);
            UNLOCK_TCPIP_CORE();
            break;
        default:
            break;
        }
        g_current_iperf_session = NULL;
        g_current_iperf_kind = IPERF_SESSION_NONE;
    }
}

// 参数表结构定义
static struct {
    struct arg_lit *server;    // 服务器模式
    struct arg_lit *client;    // 客户端模式
    struct arg_lit *stop;      // 停止会话
    struct arg_str *server_ip; // 服务器IP地址
    struct arg_int *port;      // 端口号
    struct arg_lit *dual;      // 双向测试（同时）
    struct arg_lit *tradeoff;  // 双向测试（单独）
    struct arg_int *time;      // 测试时间（秒）
    struct arg_int *interval;  // 报告间隔（秒）
    struct arg_str *window;    // TCP窗口大小
    struct arg_end *end;       // 结束标记
    struct arg_lit *udp;
    struct arg_str *bandwidth;
    struct arg_str *mode;
} iperf_args;

// 主iperf命令处理函数
static int iperf_cmd(int argc, char **argv)
{
    // 初始化参数表
    iperf_args.server = arg_lit0("s", NULL, "Run in server mode");
    iperf_args.client = arg_lit0("c", NULL, "Run in client mode");
    iperf_args.stop = arg_lit0("k", NULL, "Stop current iperf session");
    iperf_args.server_ip = arg_str0(NULL, NULL, "<server_ip>", "Server IP address (for client mode)");
    iperf_args.port = arg_int0("p", NULL, "<port>", "Server port (default: 5001)");
    iperf_args.dual = arg_lit0("d", NULL, "Do a bidirectional test simultaneously");
    iperf_args.tradeoff = arg_lit0("r", NULL, "Do a bidirectional test individually");
    iperf_args.udp = arg_lit0("u", NULL, "Use UDP instead of TCP");
    iperf_args.bandwidth = arg_str0("b", "bandwidth", "<rate>", "UDP target bandwidth, e.g. 100M");
    iperf_args.time = arg_int0("t", "time", "<time>", "Time in seconds to transmit for (default: 10)");
    iperf_args.interval = arg_int0("i", "interval", "<interval>", "Seconds between periodic bandwidth reports (default: 1)");
    iperf_args.window = arg_str0("w", "window", "<window>", "TCP window size (default: 8KB)");
    iperf_args.mode = arg_str0("m", "mode", "<raw|socket>", "API mode (default: TCP raw, UDP socket)");
    iperf_args.end = arg_end(20);

    // 创建参数表数组
    void *argtable[] = {
        iperf_args.server,
        iperf_args.client,
        iperf_args.stop,
        iperf_args.server_ip,
        iperf_args.port,
        iperf_args.dual,
        iperf_args.tradeoff,
        iperf_args.udp,
        iperf_args.bandwidth,
        iperf_args.time,
        iperf_args.interval,
        iperf_args.window,
        iperf_args.mode,
        iperf_args.end,
    };

    // 解析命令行参数
    int nerrors = arg_parse(argc, argv, argtable);

    // 检查参数解析错误
    if (nerrors != 0) {
        arg_print_errors(stderr, iperf_args.end, argv[0]);
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    if (iperf_args.interval->count > 0 || iperf_args.window->count > 0 ||
        (iperf_args.time->count > 0 && iperf_args.udp->count == 0) ||
        (iperf_args.bandwidth->count > 0 && iperf_args.udp->count == 0)) {
        printf("Error: -t and -b are supported only for UDP; -i and -w are not supported.\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    if (iperf_args.udp->count > 0 &&
        (iperf_args.dual->count > 0 || iperf_args.tradeoff->count > 0)) {
        printf("Error: -d and -r are supported only for TCP.\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    if (iperf_args.time->count > 0 && iperf_args.time->ival[0] <= 0) {
        printf("Error: UDP test time must be greater than zero.\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    if (iperf_args.bandwidth->count > 0) {
        u32_t unused_bandwidth;
        if (!iperf_parse_bandwidth(iperf_args.bandwidth->sval[0],
                                   &unused_bandwidth)) {
            printf("Error: invalid UDP bandwidth: %s\n",
                   iperf_args.bandwidth->sval[0]);
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 1;
        }
    }

    // 处理服务器模式
    if (iperf_args.mode->count > 0 &&
        strcmp(iperf_args.mode->sval[0], "raw") != 0 &&
        strcmp(iperf_args.mode->sval[0], "socket") != 0) {
        printf("Error: --mode must be 'raw' or 'socket'\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 1;
    }

    if (iperf_args.server->count > 0) {
        u16_t port = (iperf_args.port->count > 0) ? (u16_t)iperf_args.port->ival[0] : LWIPERF_TCP_PORT_DEFAULT;

        // 停止当前运行的会话（如果有）
        iperf_stop_session();

        if (iperf_args.udp->count > 0) {
            int use_raw = (iperf_args.mode->count > 0 &&
                           strcmp(iperf_args.mode->sval[0], "raw") == 0);
            printf("Starting UDP iperf server on port %d (%s API)...\n",
                   port, use_raw ? "raw" : "socket");
            if (use_raw) {
                LOCK_TCPIP_CORE();
                g_current_iperf_session = udp_raw_perf_start_server(
                    port, iperf_udp_report, NULL);
                UNLOCK_TCPIP_CORE();
                g_current_iperf_kind = IPERF_SESSION_UDP_RAW;
            } else {
                g_current_iperf_session = udp_perf_start_server(
                    port, iperf_udp_report, NULL);
                g_current_iperf_kind = IPERF_SESSION_UDP_SOCKET;
            }
            if (g_current_iperf_session == NULL) {
                g_current_iperf_kind = IPERF_SESSION_NONE;
                printf("Failed to start UDP iperf server\n");
                arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
                return 1;
            }
            printf("UDP iperf server started. Use 'iperf -k' to stop.\n");
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 0;
        }

        // 显示用户设置的参数
        {
            int use_socket = (iperf_args.mode->count > 0 &&
                              strcmp(iperf_args.mode->sval[0], "socket") == 0);
            printf("Starting TCP iperf server on port %d (%s API)...\n",
                   port, use_socket ? "netconn zero-copy" : "raw");
            if (use_socket) {
                g_current_iperf_session = tcp_socket_perf_start_server(
                    port, iperf_report, NULL);
                g_current_iperf_kind = IPERF_SESSION_TCP_SOCKET;
            } else {
                LOCK_TCPIP_CORE();
                g_current_iperf_session = lwiperf_start_tcp_server(
                    IP_ADDR_ANY, port, iperf_report, NULL);
                UNLOCK_TCPIP_CORE();
                g_current_iperf_kind = IPERF_SESSION_TCP_RAW;
            }
        }

        if (g_current_iperf_session == NULL) {
            g_current_iperf_kind = IPERF_SESSION_NONE;
            printf("Failed to start iperf server\n");
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 1;
        }

        printf("iperf server started. Use 'iperf -k' to stop.\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 0;
    }

    // 处理客户端模式
    if (iperf_args.client->count > 0) {
        if (iperf_args.server_ip->count == 0) {
            printf("Error: Server IP address is required for client mode\n");
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 1;
        }

        u16_t port = (iperf_args.port->count > 0) ? (u16_t)iperf_args.port->ival[0] : LWIPERF_TCP_PORT_DEFAULT;
        enum lwiperf_client_type type = LWIPERF_CLIENT;

        if (iperf_args.dual->count > 0) {
            type = LWIPERF_DUAL;
        } else if (iperf_args.tradeoff->count > 0) {
            type = LWIPERF_TRADEOFF;
        }

        // 停止当前运行的会话（如果有）
        iperf_stop_session();

        // 解析服务器IP地址
        ip_addr_t server_addr;
        if (inet_aton(iperf_args.server_ip->sval[0], &server_addr) == 0) {
            printf("Invalid server IP address: %s\n", iperf_args.server_ip->sval[0]);
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 1;
        }

        // 显示用户设置的参数
        if (iperf_args.udp->count > 0) {
            u32_t bandwidth_bps = 1000000U;
            u32_t duration_sec = (iperf_args.time->count > 0) ?
                                 (u32_t)iperf_args.time->ival[0] : 10U;

            if (iperf_args.bandwidth->count > 0) {
                (void)iperf_parse_bandwidth(iperf_args.bandwidth->sval[0],
                                             &bandwidth_bps);
            }

            {
                int use_raw = (iperf_args.mode->count > 0 &&
                               strcmp(iperf_args.mode->sval[0], "raw") == 0);
                printf("Starting UDP iperf client to %s:%d (%s API)...\n",
                       iperf_args.server_ip->sval[0], port,
                       use_raw ? "raw" : "socket");
                if (use_raw) {
                    LOCK_TCPIP_CORE();
                    g_current_iperf_session = udp_raw_perf_start_client(
                        &server_addr, port, bandwidth_bps, duration_sec,
                        iperf_udp_report, NULL);
                    UNLOCK_TCPIP_CORE();
                    g_current_iperf_kind = IPERF_SESSION_UDP_RAW;
                } else {
                    g_current_iperf_session = udp_perf_start_client(
                        &server_addr, port, bandwidth_bps, duration_sec,
                        iperf_udp_report, NULL);
                    g_current_iperf_kind = IPERF_SESSION_UDP_SOCKET;
                }
            }
            printf("Bandwidth: %u bit/s\n", bandwidth_bps);
            printf("Time: %u seconds\n", duration_sec);
            if (g_current_iperf_session == NULL) {
                g_current_iperf_kind = IPERF_SESSION_NONE;
                printf("Failed to start UDP iperf client\n");
                arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
                return 1;
            }
            printf("UDP iperf client started. Use 'iperf -k' to stop.\n");
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 0;
        }

        if (iperf_args.mode->count > 0 &&
            strcmp(iperf_args.mode->sval[0], "socket") == 0) {
            printf("Error: TCP netconn mode currently supports server mode only\n");
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 1;
        }

        printf("Starting iperf client to %s:%d (raw API)...\n",
               iperf_args.server_ip->sval[0], port);
        printf("Type: %s\n",
               (type == LWIPERF_CLIENT) ? "Unidirectional" :
               (type == LWIPERF_DUAL) ? "Bidirectional (simultaneous)" : "Bidirectional (individual)");
        printf("Time: 10 seconds\n");
        printf("Report: final result only\n");

        LOCK_TCPIP_CORE();
        g_current_iperf_session = lwiperf_start_tcp_client(&server_addr, port, type, iperf_report, NULL);
        UNLOCK_TCPIP_CORE();
        g_current_iperf_kind = IPERF_SESSION_TCP_RAW;

        if (g_current_iperf_session == NULL) {
            g_current_iperf_kind = IPERF_SESSION_NONE;
            printf("Failed to start iperf client\n");
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 1;
        }

        printf("iperf client started. Use 'iperf -k' to stop.\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 0;
    }

    // 处理停止命令
    if (iperf_args.stop->count > 0) {
        if (g_current_iperf_session == NULL) {
            printf("No iperf session is currently running\n");
            arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
            return 1;
        }

        iperf_stop_session();
        printf("iperf session aborted\n");
        arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
        return 0;
    }

    // 如果没有匹配的命令，显示帮助信息
    printf("Usage:\n");
    printf("  iperf -s [-u] [-p <port>] [-m raw|socket]\n");
    printf("  iperf -c <server_ip> [-u] [-p <port>] [-m raw|socket] [-b <rate>] [-t <sec>]\n");
    printf("  iperf -k                    # Stop current session\n");

    // 释放参数表内存
    arg_freetable(argtable, sizeof(argtable)/sizeof(argtable[0]));
    return 1;
}

// 注册iperf命令
void register_iperf_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = "iperf",
        .help = "Network performance measurement tool",
        .hint = NULL,
        .func = &iperf_cmd,
    };
    esp_console_cmd_register(&cmd);
}
