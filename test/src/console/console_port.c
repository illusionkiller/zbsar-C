
#include "shell.h"
#include "telnet.h"
extern console_backend_t console_backend_uart;
extern console_backend_t console_backend_telnet;

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
    return (s32)shell_console_read((int)fd, (char *)buf, (size_t)nbytes);
}

sint32 _write(sint32 fd, char8 *buf, sint32 nbytes)
{
    if (nbytes <= 0) {
        return 0;
    }
    return (sint32)shell_console_write((int)fd, (const char *)buf, (size_t)nbytes);
}

int console_init(void)
{
    telnet_register_event_callback(console_telnet_event, NULL);

    if (console_backend_register(&console_backend_uart) != 0 ||
        console_backend_init(&console_backend_uart) != 0 ||
        console_backend_switch("uart") != 0)
    {
        return -1;
    }

    if (shell_start(NULL) != 0)
    {
        console_backend_deinit(&console_backend_telnet);
        console_backend_deinit(&console_backend_uart);
        return -1;
    }

    return 0;
}
