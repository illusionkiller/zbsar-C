/*
 * Copyright (C) 2016 - 2019 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */


#include <stdbool.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "list.h"
#include "netif/xadapter.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwipopts.h"
#include "dtu.h"
#include "nanopb/pb_decode.h"
#include "log.h"

extern struct netif net_interface;

#define SERVER_PORT 5000

static struct sockaddr_in client_addr = {0};
static int udp_socket = -1;
static SemaphoreHandle_t dtu_mutex = NULL;
static bool dtu_client_connected = false;
static List_t dtu_handler_list = {0};
static bool dtu_handler_list_initialized = false;

typedef struct
{
    ListItem_t list_item;
    ChannelType type;
    pb_size_t which_values;
    dtu_message_handler_t handler;
    void *user_data;
} dtu_handler_node_t;

static bool dtu_ensure_mutex(void)
{
    if (dtu_mutex != NULL)
    {
        return true;
    }

    dtu_mutex = xSemaphoreCreateRecursiveMutex();
    return dtu_mutex != NULL;
}

static bool dtu_ensure_handler_list(void)
{
    if (!dtu_ensure_mutex())
    {
        return false;
    }

    if (!dtu_handler_list_initialized)
    {
        vListInitialise(&dtu_handler_list);
        dtu_handler_list_initialized = true;
    }

    return true;
}

static bool dtu_take_lock(void)
{
    return dtu_ensure_mutex() && xSemaphoreTakeRecursive(dtu_mutex, portMAX_DELAY) == pdPASS;
}

static void dtu_release_lock(void)
{
    if (dtu_mutex != NULL)
    {
        xSemaphoreGiveRecursive(dtu_mutex);
    }
}

static bool dtu_client_is_ready(void)
{
    bool ready = false;

    if (!dtu_take_lock())
    {
        return false;
    }

    ready = dtu_client_connected &&
            udp_socket >= 0 &&
            client_addr.sin_family == AF_INET &&
            client_addr.sin_port != 0 &&
            client_addr.sin_addr.s_addr != 0;
    dtu_release_lock();

    return ready;
}

static dtu_handler_node_t *dtu_find_handler_locked(ChannelType type, pb_size_t which_values,
                                                  dtu_message_handler_t handler, void *user_data)
{
    ListItem_t *item = listGET_HEAD_ENTRY(&dtu_handler_list);
    const ListItem_t *end = listGET_END_MARKER(&dtu_handler_list);

    while (item != end)
    {
        dtu_handler_node_t *node = (dtu_handler_node_t *)listGET_LIST_ITEM_OWNER(item);
        if (node != NULL &&
            node->type == type &&
            node->which_values == which_values &&
            node->handler == handler &&
            node->user_data == user_data)
        {
            return node;
        }
        item = listGET_NEXT(item);
    }

    return NULL;
}

static int dtu_dispatch_message(OneOfMessage *msg)
{
    typedef struct
    {
        dtu_message_handler_t handler;
        void *user_data;
    } dtu_dispatch_target_t;
    dtu_dispatch_target_t *targets = NULL;
    size_t target_count = 0U;
    size_t target_index = 0U;

    if (msg == NULL)
    {
        return -1;
    }

    if (!dtu_ensure_handler_list() || !dtu_take_lock())
    {
        return -1;
    }

    ListItem_t *item = listGET_HEAD_ENTRY(&dtu_handler_list);
    const ListItem_t *end = listGET_END_MARKER(&dtu_handler_list);

    while (item != end)
    {
        dtu_handler_node_t *node = (dtu_handler_node_t *)listGET_LIST_ITEM_OWNER(item);
        ListItem_t *next = listGET_NEXT(item);
        if (node != NULL &&
            node->type == msg->type &&
            node->which_values == msg->which_values &&
            node->handler != NULL)
        {
            ++target_count;
        }
        item = next;
    }
    if (target_count > 0U)
    {
        targets = pvPortMalloc(target_count * sizeof(*targets));
        if (targets == NULL)
        {
            dtu_release_lock();
            return -1;
        }
        item = listGET_HEAD_ENTRY(&dtu_handler_list);
        while (item != end)
        {
            dtu_handler_node_t *node = (dtu_handler_node_t *)listGET_LIST_ITEM_OWNER(item);
            if (node != NULL && node->type == msg->type &&
                node->which_values == msg->which_values && node->handler != NULL)
            {
                targets[target_index].handler = node->handler;
                targets[target_index].user_data = node->user_data;
                ++target_index;
            }
            item = listGET_NEXT(item);
        }
    }
    dtu_release_lock();

    /* A handler can wait for another worker; never hold the DTU registry lock
     * across that call or a worker's dtu_send_data() can deadlock with it. */
    for (size_t i = 0U; i < target_count; ++i)
    {
        if (targets[i].handler(msg, targets[i].user_data) == 0)
        {
            vPortFree(targets);
            return 0;
        }
    }
    vPortFree(targets);

    log_warn("DTU message unhandled, type=%d which=%u", (int)msg->type, (unsigned int)msg->which_values);
    return -1;
}

int dtu_register_message_handler(ChannelType type, pb_size_t which_values,
                                 dtu_message_handler_t handler, void *user_data)
{
    if (handler == NULL)
    {
        return -1;
    }

    if (!dtu_ensure_handler_list() || !dtu_take_lock())
    {
        return -1;
    }

    if (dtu_find_handler_locked(type, which_values, handler, user_data) != NULL)
    {
        dtu_release_lock();
        return 0;
    }

    dtu_handler_node_t *node = pvPortMalloc(sizeof(*node));
    if (node == NULL)
    {
        dtu_release_lock();
        return -1;
    }

    memset(node, 0, sizeof(*node));
    vListInitialiseItem(&node->list_item);
    listSET_LIST_ITEM_OWNER(&node->list_item, node);
    node->type = type;
    node->which_values = which_values;
    node->handler = handler;
    node->user_data = user_data;
    vListInsertEnd(&dtu_handler_list, &node->list_item);
    dtu_release_lock();
    return 0;
}

int dtu_unregister_message_handler(ChannelType type, pb_size_t which_values,
                                   dtu_message_handler_t handler, void *user_data)
{
    if (!dtu_ensure_handler_list() || !dtu_take_lock())
    {
        return -1;
    }

    dtu_handler_node_t *node = dtu_find_handler_locked(type, which_values, handler, user_data);
    if (node != NULL)
    {
        uxListRemove(&node->list_item);
        dtu_release_lock();
        vPortFree(node);
        return 0;
    }

    dtu_release_lock();
    return -1;
}

static void dtu_udp_thread(void *arg)
{
    (void)arg;

    struct sockaddr_in server_addr;
    static uint8_t buffer[OneOfMessage_size] = {0};
    OneOfMessage *msg = pvPortMalloc(sizeof(*msg));

    if (msg == NULL)
    {
        log_error("Failed to allocate DTU message buffer");
        vTaskDelete(NULL);
        return;
    }
    memset(msg, 0, sizeof(*msg));

    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd < 0)
    {
        log_error("Failed to create socket, err:%d", errno);
        vPortFree(msg);
        vTaskDelete(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        log_error("Failed to bind socket, err:%d", errno);
        close(socket_fd);
        vPortFree(msg);
        vTaskDelete(NULL);
        return;
    }

    if (!dtu_take_lock())
    {
        close(socket_fd);
        vPortFree(msg);
        vTaskDelete(NULL);
        return;
    }
    udp_socket = socket_fd;
    dtu_client_connected = false;
    memset(&client_addr, 0, sizeof(client_addr));
    dtu_release_lock();

    log_info("dtu udp server started ip %s listening on port %d", inet_ntoa(net_interface.ip_addr), SERVER_PORT);

    while (1)
    {
        struct sockaddr_in remote_addr = {0};
        socklen_t client_addr_len = sizeof(remote_addr);
        int len = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&remote_addr, &client_addr_len);
        if (len > 0)
        {
            if (strncmp((char *)buffer, "$connect$", strlen("$connect$")) == 0)
            {
                if (dtu_take_lock())
                {
                    client_addr = remote_addr;
                    dtu_client_connected = true;
                    dtu_release_lock();
                }

                sendto(socket_fd, "$ok$", strlen("$ok$"), 0,
                       (struct sockaddr *)&remote_addr, sizeof(remote_addr));
                continue;
            }

            if (!dtu_client_is_ready())
            {
//                log_debug("DTU client not connected, ignore packet from %s:%u",
//                         inet_ntoa(remote_addr.sin_addr), ntohs(remote_addr.sin_port));
                continue;
            }

            pb_istream_t stream = pb_istream_from_buffer(buffer, len);

            memset(msg, 0, sizeof(*msg));
            if (!pb_decode(&stream, OneOfMessage_fields, msg))
            {
                log_error("Failed to decode dtu data, err:%s", PB_GET_ERROR(&stream));
                continue;
            }

            if (dtu_dispatch_message(msg) != 0)
            {
                log_warn("DTU message dispatch failed, type=%d which=%u",
                         (int)msg->type, (unsigned int)msg->which_values);
            }
        }
        else
        {
            log_error("Failed to receive dtu data, err:%d", errno);
            break;
        }
    }

    log_error("eception,exit dtu thread");
    if (dtu_take_lock())
    {
        int socket_to_close = udp_socket;
        udp_socket = -1;
        dtu_client_connected = false;
        memset(&client_addr, 0, sizeof(client_addr));
        dtu_release_lock();
        close(socket_to_close);
    }
    else
    {
        close(socket_fd);
        udp_socket = -1;
    }
    vPortFree(msg);
    vTaskDelete(NULL);
}

int dtu_send_data(const uint8_t *data, uint32_t len)
{
    int bytes_sent = -1;
    int socket_to_send = -1;
    struct sockaddr_in target = {0};

    if (data == NULL || len == 0)
    {
        return -1;
    }

    if (!dtu_take_lock())
    {
        return -1;
    }

    if (udp_socket >= 0 && dtu_client_connected &&
        client_addr.sin_family == AF_INET &&
        client_addr.sin_port != 0 &&
        client_addr.sin_addr.s_addr != 0)
    {
        socket_to_send = udp_socket;
        target = client_addr;
    }
    dtu_release_lock();

    if (socket_to_send < 0)
    {
        return -1;
    }

    bytes_sent = sendto(socket_to_send, data, len, 0,
                        (struct sockaddr *)&target, sizeof(target));
    if (bytes_sent < 0)
    {
        log_error("Failed to send dtu data, err:%d", errno);
    }
    return bytes_sent;
}

void start_dtu_server(void)
{
    if (!dtu_ensure_handler_list())
    {
        log_error("Failed to initialize DTU handler list");
        return;
    }

    sys_thread_new("dtu_udp", dtu_udp_thread, NULL,
                   configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 8);
    vTaskDelay(10 / portTICK_RATE_MS);
}
