/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

#include "console.h"

#define CONSOLE_PROMPT_MAX_LEN (32)


#define CONSOLE_PATH_MAX_LEN   (128)


typedef enum {
    CONSOLE_REPL_STATE_DEINIT,
    CONSOLE_REPL_STATE_INIT,
    CONSOLE_REPL_STATE_START,
} repl_state_t;

typedef struct {
    esp_console_repl_t repl_core;        // base class
    char prompt[CONSOLE_PROMPT_MAX_LEN]; // Prompt to be printed before each line
    repl_state_t state;
    const char *history_save_path;
    TaskHandle_t task_hdl;              // REPL task handle
    size_t max_cmdline_length;          // Maximum length of a command line. If 0, default value will be used.
} esp_console_repl_com_t;

typedef struct {
    esp_console_repl_com_t repl_com; // base class
    int fd;                // file descriptor for console backend
} esp_console_repl_universal_t;

void esp_console_repl_task(void *args);

int esp_console_common_init(size_t max_cmdline_length, esp_console_repl_com_t *repl_com);
int esp_console_setup_prompt(const char *prompt, esp_console_repl_com_t *repl_com);
int esp_console_setup_history(const char *history_path,
                                    uint32_t max_history_len,
                                    esp_console_repl_com_t *repl_com);
