#include "../../shell/src/console_backend.h"

#include <string.h>

static console_backend_t *registered_backends[CONSOLE_BACKEND_MAX];
static int backend_count = 0;
static console_backend_t *default_backend = NULL;
static console_backend_t *active_backend = NULL;
static SemaphoreHandle_t s_backend_mutex = NULL;
static SemaphoreHandle_t s_stdio_write_mutex = NULL;

static SemaphoreHandle_t console_backend_mutex_handle(void)
{
    if (s_backend_mutex == NULL) {
        s_backend_mutex = xSemaphoreCreateMutex();
    }
    return s_backend_mutex;
}

static void console_backend_lock(void)
{
    SemaphoreHandle_t mutex = console_backend_mutex_handle();
    if (mutex) {
        xSemaphoreTake(mutex, portMAX_DELAY);
    }
}

static void console_backend_unlock(void)
{
    if (s_backend_mutex) {
        xSemaphoreGive(s_backend_mutex);
    }
}

static SemaphoreHandle_t console_stdio_write_mutex_handle(void)
{
    if (s_stdio_write_mutex == NULL) {
        s_stdio_write_mutex = xSemaphoreCreateMutex();
    }
    return s_stdio_write_mutex;
}

static console_backend_t *console_backend_find_unlocked(const char *name)
{
    if (name == NULL) return NULL;
    for (int i = 0; i < backend_count; i++) {
        if (strcmp(registered_backends[i]->name, name) == 0)
            return registered_backends[i];
    }
    return NULL;
}

int console_backend_register(console_backend_t *backend)
{
    console_backend_lock();

    if (backend == NULL || backend_count >= CONSOLE_BACKEND_MAX) {
        console_backend_unlock();
        return -1;
    }

    registered_backends[backend_count++] = backend;
    if (default_backend == NULL)
        default_backend = backend;

    console_backend_unlock();
    return 0;
}

console_backend_t *console_backend_find(const char *name)
{
    console_backend_t *backend;

    console_backend_lock();
    backend = console_backend_find_unlocked(name);
    console_backend_unlock();

    return backend;
}

int console_backend_switch(const char *name)
{
    console_backend_t *backend;

    console_backend_lock();
    backend = console_backend_find_unlocked(name);
    if (backend == NULL) {
        console_backend_unlock();
        return -1;
    }

    active_backend = backend;

    console_backend_unlock();
    return 0;
}

console_backend_t *console_backend_get_current(void)
{
    console_backend_t *backend;

    console_backend_lock();
    backend = active_backend ? active_backend : default_backend;
    console_backend_unlock();

    return backend;
}

void console_backend_set_default(console_backend_t *backend)
{
    console_backend_lock();
    default_backend = backend;
    console_backend_unlock();
}

void console_backend_deinit(console_backend_t *backend)
{
    if (backend == NULL) return;
    backend->ops->deinit(backend);
}

int console_backend_init(console_backend_t *backend)
{
    if (backend == NULL) return -1;
    return backend->ops->init(backend);
}

size_t console_backend_read(console_backend_t *backend, char *buf, size_t len, TickType_t timeout)
{
    if (backend == NULL) return 0;
    return backend->ops->read(backend, buf, len, timeout);
}

size_t console_backend_write(console_backend_t *backend, char *buf, size_t len, TickType_t timeout)
{
    if (backend == NULL) return 0;
    return backend->ops->write(backend, buf, len, timeout);
}

int shell_console_read(int fd, char *buf, size_t len)
{
    console_backend_t *backend;
    size_t read_count = 0;

    if (fd != 0 || buf == NULL || len == 0) {
        return -1;
    }

    backend = console_backend_get_current();
    if (backend == NULL || backend->ops == NULL || backend->ops->read == NULL) {
        return -1;
    }

    while (read_count < len) {
        if (console_backend_read(backend, &buf[read_count], 1, pdMS_TO_TICKS(10)) == 0) {
            continue;
        }

        read_count++;
        if (buf[read_count - 1] == '\n' || buf[read_count - 1] == '\r') {
            break;
        }
    }

    return (int)read_count;
}

int shell_console_write(int fd, const char *buf, size_t len)
{
    console_backend_t *backend;
    SemaphoreHandle_t mutex;
    size_t written = 0;

    if ((fd != 1 && fd != 2) || buf == NULL || len == 0) {
        return -1;
    }

    backend = console_backend_get_current();
    if (backend == NULL || backend->ops == NULL || backend->ops->write == NULL) {
        return -1;
    }

    mutex = console_stdio_write_mutex_handle();
    if (mutex == NULL || xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return -1;
    }

    while (written < len) {
        if (buf[written] == '\n') {
            char cr = '\r';
            if (console_backend_write(backend, &cr, 1, portMAX_DELAY) != 1) {
                break;
            }
        }
        if (console_backend_write(backend, (char *)&buf[written], 1, portMAX_DELAY) != 1) {
            break;
        }
        written++;
    }

    xSemaphoreGive(mutex);
    return written == len ? (int)written : -1;
}
