#ifndef __SHELL_H__
#define __SHELL_H__

#include "../../shell/src/argtable3.h"
#include "../../shell/src/console.h"
#include "../../shell/src/console_backend.h"
/* Backends must be registered and selected before starting the REPL. */
int shell_start(const esp_console_repl_config_t *config);

#endif // __SHELL_H__
