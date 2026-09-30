/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <sys/cdefs.h> // __containerof
#include "console.h"
#include <stdio.h>
#include <stdlib.h>
#include "console_private.h"
#include "linenoise.h"

int esp_console_setup_prompt(const char *prompt, esp_console_repl_com_t *repl_com)
{
    /* set command line prompt */
    const char *prompt_temp = "xilinx>";
    if (prompt) {
        prompt_temp = prompt;
    }
    snprintf(repl_com->prompt, CONSOLE_PROMPT_MAX_LEN - 1, "%s", prompt_temp);

    return 0;
}

int esp_console_setup_history(const char *history_path, uint32_t max_history_len, esp_console_repl_com_t *repl_com)
{
    int ret = 0;

    if (history_path) {
        /* Load command history from filesystem */
        linenoiseHistoryLoad(history_path);
    }

    /* Set command history size */
    if (linenoiseHistorySetMaxLen(max_history_len) != 1) {
//        ESP_LOGE(TAG, "set max history length to %"PRIu32" failed", max_history_len);
        ret = -1;
        goto _exit;
    }
    return 0;
_exit:
    return ret;
}

int esp_console_common_init(size_t max_cmdline_length, esp_console_repl_com_t *repl_com)
{
    int ret = 0;
    /* Initialize the console */
    esp_console_config_t console_config = ESP_CONSOLE_CONFIG_DEFAULT();
    repl_com->max_cmdline_length = console_config.max_cmdline_length;
    /* Replace the default command line length if passed as a parameter */
    if (max_cmdline_length != 0) {
        console_config.max_cmdline_length = max_cmdline_length;
        repl_com->max_cmdline_length = max_cmdline_length;
    }

#if CONFIG_LOG_COLORS
    console_config.hint_color = atoi(LOG_COLOR_CYAN);
#else
    console_config.hint_color = -1;
#endif
    ret = esp_console_init(&console_config);
    if (ret != 0) {
        goto _exit;
    }

    ret = esp_console_register_help_command();
    if (ret != 0) {
        goto _exit;
    }

    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within single line */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback *)&esp_console_get_hint);

    return 0;
_exit:
    return ret;
}
