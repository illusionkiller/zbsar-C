
#include "shell.h"
#include "telnet.h"
#include "xil_printf.h"

extern console_backend_t console_backend_uart;
extern console_backend_t console_backend_telnet;

static volatile int console_backend_ready;
extern s32 console_early_read(s32 fd, char8 *buf, s32 nbytes);
extern sint32 console_early_write(sint32 fd, char8 *buf, sint32 nbytes);
static void console_telnet_event(void *user_data, telnet_event_t event)
{
    (void)user_data;

    if (event == TELNET_EVENT_CONNECTED) {
        (void)console_backend_switch("telnet");
    } else if (event == TELNET_EVENT_DISCONNECTED) {
        (void)console_backend_switch("uart");
    }
}

s32 _read(s32 fd, char8 *buf, s32 nbytes)
{
    if (nbytes <= 0) {
        return 0;
    }

    if (!console_backend_ready) {
        return console_early_read(fd, buf, nbytes);
    }

    return (s32)shell_console_read((int)fd, (char *)buf, (size_t)nbytes);
}

s32 read(s32 fd, char8 *buf, s32 nbytes)
{
    if (nbytes <= 0) {
        return 0;
    }

    if (!console_backend_ready) {
        return console_early_read(fd, buf, nbytes);
    }

    return (s32)shell_console_read((int)fd, (char *)buf, (size_t)nbytes);
}

sint32 _write(sint32 fd, char8 *buf, sint32 nbytes)
{
    if (nbytes <= 0) {
        return 0;
    }

    if (!console_backend_ready) {
        return console_early_write(fd, buf, nbytes);
    }

    return (sint32)shell_console_write((int)fd, (const char *)buf, (size_t)nbytes);
}

sint32 write(sint32 fd, char8 *buf, sint32 nbytes)
{
    if (nbytes <= 0) {
        return 0;
    }

    if (!console_backend_ready) {
        return console_early_write(fd, buf, nbytes);
    }

    return (sint32)shell_console_write((int)fd, (const char *)buf, (size_t)nbytes);
}

int console_init(void)
{
    if (console_backend_init(&console_backend_uart) != 0)
    {
        return -1;
    }

    if (console_backend_register(&console_backend_uart) != 0 ||
        console_backend_switch("uart") != 0)
    {
        console_backend_deinit(&console_backend_uart);
        return -1;
    }

    if (console_backend_init(&console_backend_telnet) != 0)
    {
        console_backend_deinit(&console_backend_uart);
        return -1;
    }

    if (console_backend_register(&console_backend_telnet) != 0)
    {
        console_backend_deinit(&console_backend_telnet);
        console_backend_deinit(&console_backend_uart);
        return -1;
    }

    telnet_register_event_callback(console_telnet_event, NULL);
    console_backend_ready = 1;

    if (shell_start(NULL) != 0)
    {
        console_backend_ready = 0;
        console_backend_deinit(&console_backend_telnet);
        console_backend_deinit(&console_backend_uart);
        return -1;
    }

    if (telnet_service_start() != 0)
    {
        return -1;
    }

    return 0;
}
