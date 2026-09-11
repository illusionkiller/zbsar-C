#ifndef _TELNET_H
#define _TELNET_H
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#define TELNET_PORT            23
#define TELNET_SERVER_STACK    4096
#define TELNET_RX_BUFFER_SIZE  256
#define TELNET_RECV_BUF_SIZE   64
#define STATE_NORMAL  0
#define STATE_IAC     1
#define STATE_WILL    2
#define STATE_WONT    3
#define STATE_DO      4
#define STATE_DONT    5

#define TELNET_IAC    255
#define TELNET_WILL   251
#define TELNET_WONT   252
#define TELNET_DO     253
#define TELNET_DONT   254

#define TELNET_OPT_ECHO  1
#define TELNET_OPT_SGA   3

typedef enum {
    TELNET_EVENT_CONNECTED,
    TELNET_EVENT_DISCONNECTED,
} telnet_event_t;

typedef void (*telnet_event_callback_t)(void *user_data, telnet_event_t event);

typedef struct {
    volatile int client_fd;
    uint8_t rx_buf[TELNET_RX_BUFFER_SIZE];
    volatile size_t rx_head;
    volatile size_t rx_tail;
    uint8_t state;
    uint8_t drop_cr_followup;
    SemaphoreHandle_t rx_sem;
} telnet_session_t;

extern telnet_session_t telnet_session;
size_t telnet_rb_get(telnet_session_t *s, char *buf, size_t len);

/**
 * Register the callback notified when a Telnet client connects or disconnects.
 * Passing NULL unregisters the callback.
 */
void telnet_register_event_callback(telnet_event_callback_t callback,
                                     void *user_data);

int telnet_service_start(void);

#endif /* APP_TELNET_SERVICE_H */
