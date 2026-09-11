/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   scpi_server.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  TCP/IP SCPI Server - Socket Implementation
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "scpi-def.h"
#include "log.h"

#define DEVICE_PORT 5025
#define CONTROL_PORT 5026

#define SCPI_THREAD_PRIO (tskIDLE_PRIORITY + 3)

#define SCPI_MSG_TIMEOUT                0
#define SCPI_MSG_IO_LISTEN              1
#define SCPI_MSG_CONTROL_IO_LISTEN      2
#define SCPI_MSG_IO                     3
#define SCPI_MSG_CONTROL_IO             4
#define SCPI_MSG_SET_ESE_REQ            5
#define SCPI_MSG_SET_ERROR              6

static const int keepidle = 30;    // 空闲30秒后开始发送keepalive探测
static const int keepintvl = 5;     // 探测间隔5秒
static const int keepcnt = 3;       // 最多发送3次探测

// 连接状态
#define CONNECTION_NONE    0
#define CONNECTION_ACTIVE  1
#define CONNECTION_CLOSING 2

typedef struct {
    int io_listen_fd;                // 设备端口监听文件描述符
    int control_io_listen_fd;        // 控制端口监听文件描述符
    int io_fd;                       // 设备端口活动连接文件描述符
    int control_io_fd;               // 控制端口活动连接文件描述符
    int io_state;                    // 设备端口连接状态
    int control_io_state;            // 控制端口连接状态
    xQueueHandle evtQueue;           // 事件队列
    xSemaphoreHandle conn_mutex;     // 连接互斥锁
    fd_set read_fds;                 // 用于select的文件描述符集合
    int max_fd;                      // 最大文件描述符
} user_data_t;

struct _queue_event_t {
    uint8_t cmd;
    uint8_t param1;
    int16_t param2;
} __attribute__((__packed__));
typedef struct _queue_event_t queue_event_t;

user_data_t user_data = {
    .io_listen_fd = -1,
    .control_io_listen_fd = -1,
    .io_fd = -1,
    .control_io_fd = -1,
    .io_state = CONNECTION_NONE,
    .control_io_state = CONNECTION_NONE,
    .evtQueue = 0,
    .conn_mutex = NULL,
    .max_fd = -1
};

/**
 * @brief SCPI写函数实现
 */
size_t SCPI_Write(scpi_t * context, const char * data, size_t len) {
    if (context->user_context != NULL) {
        user_data_t * u = (user_data_t *) (context->user_context);
        if (u->io_fd >= 0 && u->io_state == CONNECTION_ACTIVE) {
            return write(u->io_fd, data, len);
        }
    }
    return 0;
}

/**
 * @brief SCPI刷新函数实现
 */
scpi_result_t SCPI_Flush(scpi_t * context) {
    // Socket API不需要显式刷新
    return SCPI_RES_OK;
}

/**
 * @brief SCPI错误处理函数
 */
int SCPI_Error(scpi_t * context, int_fast16_t err) {
    (void) context;
    iprintf("**ERROR: %ld, \"%s\"\r\n", (int32_t) err, SCPI_ErrorTranslate(err));
    if (err != 0) {
        /* New error */
        /* Beep */
        /* Error LED ON */
    } else {
        /* No more errors in the queue */
        /* Error LED OFF */
    }
    return 0;
}

/**
 * @brief SCPI控制函数实现
 */
scpi_result_t SCPI_Control(scpi_t * context, scpi_ctrl_name_t ctrl, scpi_reg_val_t val) {
    char b[16];

    if (SCPI_CTRL_SRQ == ctrl) {
        iprintf("**SRQ: 0x%X (%d)\r\n", val, val);
    } else {
        iprintf("**CTRL %02x: 0x%X (%d)\r\n", ctrl, val, val);
    }

    if (context->user_context != NULL) {
        user_data_t * u = (user_data_t *) (context->user_context);
        if (u->control_io_fd >= 0 && u->control_io_state == CONNECTION_ACTIVE) {
            snprintf(b, sizeof(b), "SRQ%d\r\n", val);
            return write(u->control_io_fd, b, strlen(b)) >= 0 ? SCPI_RES_OK : SCPI_RES_ERR;
        }
    }
    return SCPI_RES_OK;
}

/**
 * @brief SCPI重置函数实现
 */
scpi_result_t SCPI_Reset(scpi_t * context) {
    (void) context;
    iprintf("**Reset\r\n");
    return SCPI_RES_OK;
}

/**
 * @brief SCPI获取控制端口函数
 */
scpi_result_t SCPI_SystemCommTcpipControlQ(scpi_t * context) {
    SCPI_ResultInt(context, CONTROL_PORT);
    return SCPI_RES_OK;
}

/**
 * @brief 设置ESE请求位
 */
static void setEseReq(void) {
    SCPI_RegSetBits(&scpi_context, SCPI_REG_ESR, ESR_REQ);
}

/**
 * @brief 设置错误
 */
static void setError(int16_t err) {
    SCPI_ErrorPush(&scpi_context, err);
}

/**
 * @brief 请求控制
 */
void SCPI_RequestControl(void) {
    queue_event_t msg;
    msg.cmd = SCPI_MSG_SET_ESE_REQ;

    xQueueSend(user_data.evtQueue, &msg, 1000);
}

/**
 * @brief 添加错误
 */
void SCPI_AddError(int16_t err) {
    queue_event_t msg;
    msg.cmd = SCPI_MSG_SET_ERROR;
    msg.param2 = err;

    xQueueSend(user_data.evtQueue, &msg, 1000);
}

/**
 * @brief 创建TCP服务器套接字
 */
static int createServer(int port) {
    int fd;
    struct sockaddr_in addr;
    int opt = 1;

    // 创建TCP套接字
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        log_error("Failed to create socket: %d", errno);
        return -1;
    }

    // 设置地址复用
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_warn("Failed to set SO_REUSEADDR (not supported in this environment): %d", errno);
        // 不支持此选项时继续执行，不返回错误
    }

    // 设置非阻塞模式
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        log_error("Failed to set non-blocking: %d", errno);
        close(fd);
        return -1;
    }

    // 绑定地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        log_error("Failed to bind socket: %d", errno);
        close(fd);
        return -1;
    }

    // 开始监听
    if (listen(fd, 1) < 0) {
        log_error("Failed to listen on socket: %d", errno);
        close(fd);
        return -1;
    }

    log_info("Server listening on port %d, fd=%d", port, fd);
    return fd;
}

/**
 * @brief 接受新连接
 */
static int acceptConnection(int listen_fd) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int new_fd;

    new_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
    if (new_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log_error("Failed to accept connection: %d", errno);
        }
        return -1;
    }

    // 设置keepalive选项
    int keepalive = 1;
    if (setsockopt(new_fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
        log_warn("Failed to set SO_KEEPALIVE: %d", errno);
    }

    // 设置keepalive参数
    if (setsockopt(new_fd, IPPROTO_TCP, TCP_KEEPIDLE, &keepidle, sizeof(keepidle)) < 0) {
        log_warn("Failed to set TCP_KEEPIDLE: %d", errno);
    }

    if (setsockopt(new_fd, IPPROTO_TCP, TCP_KEEPINTVL, &keepintvl, sizeof(keepintvl)) < 0) {
        log_warn("Failed to set TCP_KEEPINTVL: %d", errno);
    }

    if (setsockopt(new_fd, IPPROTO_TCP, TCP_KEEPCNT, &keepcnt, sizeof(keepcnt)) < 0) {
        log_warn("Failed to set TCP_KEEPCNT: %d", errno);
    }

    // 设置非阻塞模式
    if (fcntl(new_fd, F_SETFL, O_NONBLOCK) < 0) {
        log_error("Failed to set non-blocking on new connection: %d", errno);
        close(new_fd);
        return -1;
    }

    log_info("New connection from %s:%d, fd=%d", 
             inet_ntoa(client_addr.sin_addr), 
             ntohs(client_addr.sin_port), new_fd);

    return new_fd;
}

/**
 * @brief 关闭连接
 */
static void closeConnection(user_data_t *user_data, int *fd, int *state) {
    if (*fd >= 0) {
        log_info("Closing connection, fd=%d", *fd);
        close(*fd);
        
        // 从文件描述符集合中移除
        FD_CLR(*fd, &user_data->read_fds);
        
        *fd = -1;
        *state = CONNECTION_NONE;
        
        // 重新计算max_fd
        user_data->max_fd = -1;
        if (user_data->io_listen_fd >= 0 && user_data->io_listen_fd > user_data->max_fd) {
            user_data->max_fd = user_data->io_listen_fd;
        }
        if (user_data->control_io_listen_fd >= 0 && user_data->control_io_listen_fd > user_data->max_fd) {
            user_data->max_fd = user_data->control_io_listen_fd;
        }
        if (user_data->io_fd >= 0 && user_data->io_fd > user_data->max_fd) {
            user_data->max_fd = user_data->io_fd;
        }
        if (user_data->control_io_fd >= 0 && user_data->control_io_fd > user_data->max_fd) {
            user_data->max_fd = user_data->control_io_fd;
        }
    }
}

/**
 * @brief 处理设备端口监听事件
 */
static int processIoListen(user_data_t * user_data) {
    int new_fd;
//    struct sockaddr_in client_addr;
//    socklen_t addr_len = sizeof(client_addr);

    // 接受新连接
    new_fd = acceptConnection(user_data->io_listen_fd);
    if (new_fd < 0) {
        return -1;
    }

    // 使用互斥锁保护连接操作
    if (xSemaphoreTake(user_data->conn_mutex, portMAX_DELAY) == pdPASS) {
        // 如果已有连接，拒绝新连接
        if (user_data->io_fd >= 0 && user_data->io_state == CONNECTION_ACTIVE) {
            log_warn("Rejecting new connection, already connected");
            close(new_fd);
        } else {
            // 关闭旧连接（如果有）
            closeConnection(user_data, &user_data->io_fd, &user_data->io_state);
            
            // 设置新连接
            user_data->io_fd = new_fd;
            user_data->io_state = CONNECTION_ACTIVE;
            
            // 更新文件描述符集合
            FD_SET(user_data->io_fd, &user_data->read_fds);
            if (user_data->io_fd > user_data->max_fd) {
                user_data->max_fd = user_data->io_fd;
            }
        }
        
        xSemaphoreGive(user_data->conn_mutex);
    }

    return 0;
}

/**
 * @brief 处理控制端口监听事件
 */
static int processSrqIoListen(user_data_t * user_data) {
    int new_fd;

    // 接受新连接
    new_fd = acceptConnection(user_data->control_io_listen_fd);
    if (new_fd < 0) {
        return -1;
    }

    // 使用互斥锁保护连接操作
    if (xSemaphoreTake(user_data->conn_mutex, portMAX_DELAY) == pdPASS) {
        // 如果已有连接，拒绝新连接
        if (user_data->control_io_fd >= 0 && user_data->control_io_state == CONNECTION_ACTIVE) {
            log_warn("Rejecting new control connection, already connected");
            close(new_fd);
        } else {
            // 关闭旧连接（如果有）
            closeConnection(user_data, &user_data->control_io_fd, &user_data->control_io_state);
            
            // 设置新连接
            user_data->control_io_fd = new_fd;
            user_data->control_io_state = CONNECTION_ACTIVE;
            
            // 更新文件描述符集合
            FD_SET(user_data->control_io_fd, &user_data->read_fds);
            if (user_data->control_io_fd > user_data->max_fd) {
                user_data->max_fd = user_data->control_io_fd;
            }
        }
        
        xSemaphoreGive(user_data->conn_mutex);
    }

    return 0;
}

/**
 * @brief 处理设备端口数据
 */
static int processIo(user_data_t * user_data) {
    char buffer[1024];
    int bytes_read;

    // 读取数据
    bytes_read = read(user_data->io_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log_error("Failed to read from device socket: %d", errno);
            closeConnection(user_data, &user_data->io_fd, &user_data->io_state);
        }
        return -1;
    } else if (bytes_read == 0) {
        // 客户端关闭连接
        log_info("Device connection closed by client");
        closeConnection(user_data, &user_data->io_fd, &user_data->io_state);
        return -1;
    }

    // 处理SCPI输入
    buffer[bytes_read] = '\0';
    SCPI_Input(&scpi_context, buffer, bytes_read);

    return 0;
}

/**
 * @brief 处理控制端口数据
 */
static int processSrqIo(user_data_t * user_data) {
    char buffer[128];
    int bytes_read;

    // 读取数据
    bytes_read = read(user_data->control_io_fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            log_error("Failed to read from control socket: %d", errno);
            closeConnection(user_data, &user_data->control_io_fd, &user_data->control_io_state);
        }
        return -1;
    } else if (bytes_read == 0) {
        // 客户端关闭连接
        log_info("Control connection closed by client");
        closeConnection(user_data, &user_data->control_io_fd, &user_data->control_io_state);
        return -1;
    }

    // 控制端口数据处理（预留）
    buffer[bytes_read] = '\0';
    log_debug("Received control data: %s", buffer);

    return 0;
}

/**
 * @brief 等待事件
 */
static void waitServer(user_data_t * user_data, queue_event_t * evt) {
    struct timeval timeout;
    fd_set temp_fds;
    int ret;

    // 复制文件描述符集合
    temp_fds = user_data->read_fds;

    // 设置超时时间
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    // 等待事件
    if (user_data->max_fd >= 0) {
        ret = select(user_data->max_fd + 1, &temp_fds, NULL, NULL, &timeout);
        if (ret < 0) {
            if (errno != EINTR) {
                log_error("Select error: %d", errno);
            }
            evt->cmd = SCPI_MSG_TIMEOUT;
            return;
        } else if (ret == 0) {
            // 超时
            evt->cmd = SCPI_MSG_TIMEOUT;
            return;
        }

        // 检查设备端口监听事件
        if (FD_ISSET(user_data->io_listen_fd, &temp_fds)) {
            evt->cmd = SCPI_MSG_IO_LISTEN;
            return;
        }

        // 检查控制端口监听事件
        if (FD_ISSET(user_data->control_io_listen_fd, &temp_fds)) {
            evt->cmd = SCPI_MSG_CONTROL_IO_LISTEN;
            return;
        }

        // 检查设备端口数据事件
        if (user_data->io_fd >= 0 && FD_ISSET(user_data->io_fd, &temp_fds)) {
            evt->cmd = SCPI_MSG_IO;
            return;
        }

        // 检查控制端口数据事件
        if (user_data->control_io_fd >= 0 && FD_ISSET(user_data->control_io_fd, &temp_fds)) {
            evt->cmd = SCPI_MSG_CONTROL_IO;
            return;
        }
    }
    else{
        vTaskDelay(10 / portTICK_RATE_MS); // 没有有效的文件描述符，避免忙等待
    }

    // 默认超时
    evt->cmd = SCPI_MSG_TIMEOUT;
}

/**
 * @brief SCPI服务器线程
 */
static void scpi_server_thread(void *arg) {
    queue_event_t evt;

    (void) arg;

    // 创建事件队列
    user_data.evtQueue = xQueueCreate(10, sizeof(queue_event_t));
    if (user_data.evtQueue == NULL) {
        log_error("Failed to create event queue");
        vTaskDelete(NULL);
        return;
    }

    // 创建连接互斥锁
    user_data.conn_mutex = xSemaphoreCreateMutex();
    if (user_data.conn_mutex == NULL) {
        log_error("Failed to create connection mutex");
        vTaskDelete(NULL);
        return;
    }

    // 初始化文件描述符集合
    FD_ZERO(&user_data.read_fds);
    user_data.max_fd = -1;

    // 创建服务器套接字
    user_data.io_listen_fd = createServer(DEVICE_PORT);
    if (user_data.io_listen_fd < 0) {
        log_error("Failed to create device port server");
        vTaskDelete(NULL);
        return;
    }

    user_data.control_io_listen_fd = createServer(CONTROL_PORT);
    if (user_data.control_io_listen_fd < 0) {
        log_error("Failed to create control port server");
        closeConnection(&user_data, &user_data.io_listen_fd, &user_data.io_state);
        vTaskDelete(NULL);
        return;
    }

    // 将监听套接字添加到文件描述符集合
    FD_SET(user_data.io_listen_fd, &user_data.read_fds);
    FD_SET(user_data.control_io_listen_fd, &user_data.read_fds);
    
    user_data.max_fd = (user_data.io_listen_fd > user_data.control_io_listen_fd) ? 
                      user_data.io_listen_fd : user_data.control_io_listen_fd;

    // 初始化SCPI上下文
    SCPI_Init(&scpi_context,
            scpi_commands,
            &scpi_interface,
            scpi_units_def,
            SCPI_IDN1, SCPI_IDN2, SCPI_IDN3, SCPI_IDN4,
            scpi_input_buffer, SCPI_INPUT_BUFFER_LENGTH,
            scpi_error_queue_data, SCPI_ERROR_QUEUE_SIZE);

    scpi_context.user_context = &user_data;

    log_info("SCPI server started successfully on port %d", DEVICE_PORT);

    // 主循环
    while (1) {
        waitServer(&user_data, &evt);

        if (evt.cmd == SCPI_MSG_TIMEOUT) {
            SCPI_Input(&scpi_context, NULL, 0);
        }

        if (evt.cmd == SCPI_MSG_IO_LISTEN) {
            processIoListen(&user_data);
        }

        if (evt.cmd == SCPI_MSG_CONTROL_IO_LISTEN) {
            processSrqIoListen(&user_data);
        }

        if (evt.cmd == SCPI_MSG_IO) {
            processIo(&user_data);
        }

        if (evt.cmd == SCPI_MSG_CONTROL_IO) {
            processSrqIo(&user_data);
        }

        if (evt.cmd == SCPI_MSG_SET_ESE_REQ) {
            setEseReq();
        }

        if (evt.cmd == SCPI_MSG_SET_ERROR) {
            setError(evt.param2);
        }
    }

    vTaskDelete(NULL);
}

/**
 * @brief 初始化SCPI服务器
 */
void scpi_server_init(void) {
    sys_thread_new("SCPI", scpi_server_thread, NULL, 2 * configMINIMAL_STACK_SIZE, SCPI_THREAD_PRIO);
    vTaskDelay(10 / portTICK_RATE_MS);
}
