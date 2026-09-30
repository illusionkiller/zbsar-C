/*
 * SPDX-FileCopyrightText: 2016-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "console_private.h"
#include "linenoise.h"
#include "shell.h"

static esp_console_repl_t *s_repl;


static void console_repl_task(void *arg)
{
    esp_console_repl_universal_t *repl = (esp_console_repl_universal_t *)arg;

    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

    printf("Welcome to the Xilinx Shell!\r\n");

    while (1) {
        char *line = linenoise(repl->repl_com.prompt);
        if (line == NULL) {
            continue;
        }
        int ret;
        int err = esp_console_run(line, &ret);
        if (err == -3) {
        	printf("Command not found\r\n");
        }
        linenoiseHistoryAdd(line);
        if (repl->repl_com.history_save_path) {
            linenoiseHistorySave(repl->repl_com.history_save_path);
        }
        free(line);
    }
}

int shell_start(const esp_console_repl_config_t *config)
{
    esp_console_repl_config_t default_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();

    if (s_repl != NULL)
    {
        return -1;
    }
    return esp_console_new_repl(config != NULL ? config : &default_config, &s_repl);
}

int esp_console_new_repl(const esp_console_repl_config_t *repl_config, esp_console_repl_t **ret_repl)
{
    int ret = 0;
    esp_console_repl_universal_t *repl = NULL;
    if (!repl_config || !ret_repl) {
        ret = -1;
        goto _exit;
    }
    repl = calloc(1, sizeof(esp_console_repl_universal_t));
    if (!repl) {
        ret = -1;
        goto _exit;
    }

    ret = esp_console_common_init(repl_config->max_cmdline_length, &repl->repl_com);
    if (ret != 0) {
        goto _exit;
    }

    ret = esp_console_setup_history(repl_config->history_save_path, repl_config->max_history_len, &repl->repl_com);
    if (ret != 0) {
        goto _exit;
    }

    esp_console_setup_prompt(repl_config->prompt, &repl->repl_com);

    repl->repl_com.state = CONSOLE_REPL_STATE_INIT;

    if (xTaskCreate(console_repl_task, "console_repl", repl_config->task_stack_size,
                                repl, repl_config->task_priority, &repl->repl_com.task_hdl) != pdTRUE) {
        ret = -1;
        goto _exit;
    }

    *ret_repl = &repl->repl_com.repl_core;
    return 0;
_exit:
    if (repl) {
        esp_console_deinit();
        free(repl);
    }
    if (ret_repl) {
        *ret_repl = NULL;
    }
    return ret;
}
