#ifndef CONSOLE_BACKEND_H
#define CONSOLE_BACKEND_H

#include <stddef.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

typedef struct console_backend console_backend_t;

typedef struct console_backend_ops {
    int    (*init)(console_backend_t *backend);
    void   (*deinit)(console_backend_t *backend);
    size_t (*read)(console_backend_t *backend, char *buf, size_t len, TickType_t timeout);
    size_t (*write)(console_backend_t *backend, char *buf, size_t len, TickType_t timeout);
} console_backend_ops_t;

struct console_backend {
    const char *name;
    const console_backend_ops_t *ops;
    void *priv;
};

#define CONSOLE_BACKEND_MAX  4

/**
 * Register a backend. Returns 0 on success.
 */
int console_backend_register(console_backend_t *backend);

/**
 * Switch the global active console backend by name.
 * Returns 0 on success, -1 if not found.
 */
int console_backend_switch(const char *name);

/**
 * Get the global active backend. Returns the default backend if none set.
 */
console_backend_t *console_backend_get_current(void);

/**
 * Find a registered backend by name. Returns NULL if not found.
 */
console_backend_t *console_backend_find(const char *name);

/**
 * Set the default backend (used by tasks that haven't explicitly switched).
 */
void console_backend_set_default(console_backend_t *backend);

/**
 * Deinitialize a backend.
 */
void console_backend_deinit(console_backend_t *backend);    

/**
 * Initialize a backend.
 */
int console_backend_init(console_backend_t *backend);

/**
 * Read from a backend.
 */
size_t console_backend_read(console_backend_t *backend, char *buf, size_t len, TickType_t timeout);

/**
 * Write to a backend.
 */
size_t console_backend_write(console_backend_t *backend, char *buf, size_t len, TickType_t timeout);

/**
 * Read standard input through the active console backend.
 * Returns -1 when fd is not standard input or no backend is active.
 */
int shell_console_read(int fd, char *buf, size_t len);

/**
 * Write standard output/error through the active console backend.
 * Returns -1 when fd is not standard output/error or no backend is active.
 */
int shell_console_write(int fd, const char *buf, size_t len);


#endif /* CONSOLE_BACKEND_H */
