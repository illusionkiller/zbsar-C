/*
 * SPDX-FileCopyrightText: 2016-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/queue.h>
#include "console.h"
#include "linenoise.h"
#include "argtable3.h"
#include <stdbool.h>
#define ANSI_COLOR_DEFAULT      39      /** Default foreground color */

typedef struct cmd_item_ {
    /**
     * Command name (statically allocated by application)
     */
    const char *command;
    /**
     * Help text (statically allocated by application), may be NULL.
     */
    const char *help;
    /**
     * Hint text, usually lists possible arguments, dynamically allocated.
     * May be NULL.
     */
    char *hint;
    esp_console_cmd_func_t func;                        //!< pointer to the command handler (without user context)
    esp_console_cmd_func_with_context_t func_w_context; //!< pointer to the command handler (with user context)
    void *argtable;                                     //!< optional pointer to arg table
    void *context;                                      //!< optional pointer to user context
    SLIST_ENTRY(cmd_item_) next;                        //!< next command in the list
} cmd_item_t;

typedef void (*const fn_print_arg_t)(cmd_item_t*);

/** linked list of command structures */
static SLIST_HEAD(cmd_list_, cmd_item_) s_cmd_list;

/** run-time configuration options */
static esp_console_config_t s_config = {
    .heap_alloc_caps = MALLOC_CAP_DEFAULT
};

/** temporary buffer used for command line parsing */
static char *s_tmp_line_buf;

static const cmd_item_t *find_command_by_name(const char *name);

static esp_console_help_verbose_level_e s_verbose_level = ESP_CONSOLE_HELP_VERBOSE_LEVEL_1;

int esp_console_init(const esp_console_config_t *config)
{
    if (!config) {
        return -1;
    }
    if (s_tmp_line_buf) {
        return -1;
    }
    memcpy(&s_config, config, sizeof(s_config));
    if (s_config.hint_color == 0) {
        s_config.hint_color = ANSI_COLOR_DEFAULT;
    }
    if (s_config.heap_alloc_caps == 0) {
        s_config.heap_alloc_caps = MALLOC_CAP_DEFAULT;
    }
    s_tmp_line_buf = calloc(1, config->max_cmdline_length);
    if (s_tmp_line_buf == NULL) {
        return -2;
    }
    return 0;
}

int esp_console_deinit(void)
{
    if (!s_tmp_line_buf) {
        return -1;
    }
    free(s_tmp_line_buf);
    s_tmp_line_buf = NULL;
    cmd_item_t *it, *tmp;
    SLIST_FOREACH_SAFE(it, &s_cmd_list, next, tmp) {
        SLIST_REMOVE(&s_cmd_list, it, cmd_item_, next);
        free(it->hint);
        free(it);
    }
    return 0;
}

void esp_console_rm_item_free_hint(cmd_item_t *item)
{
    SLIST_REMOVE(&s_cmd_list, item, cmd_item_, next);
    free(item->hint);
}

int esp_console_cmd_deregister(const char *cmd_name)
{
    cmd_item_t *item = (cmd_item_t *)find_command_by_name(cmd_name);
    if (item == NULL) {
        return -1;
    }
    esp_console_rm_item_free_hint(item);
    free(item);
    return 0;
}

int esp_console_cmd_register(const esp_console_cmd_t *cmd)
{
    cmd_item_t *item = NULL;
    if (!cmd || cmd->command == NULL) {
        return -1;
    }
    if (strchr(cmd->command, ' ') != NULL) {
        return -1;
    }
    if ((cmd->func == NULL && cmd->func_w_context == NULL)
            || (cmd->func != NULL && cmd->func_w_context != NULL)) {
        return -1;
    }
    item = (cmd_item_t *)find_command_by_name(cmd->command);
    if (!item) {
        // not registered before
        item = calloc(1, sizeof(*item));
        if (item == NULL) {
            return -2;
        }
    } else {
        // remove from list and free the old hint, because we will alloc new hint for the command
        esp_console_rm_item_free_hint(item);
    }
    item->command = cmd->command;
    item->help = cmd->help;
    if (cmd->hint) {
        /* Prepend a space before the hint. It separates command name and
         * the hint. arg_print_syntax below adds this space as well.
         */
        int unused __attribute__((unused));
        unused = snprintf(item->hint, s_config.max_cmdline_length, " %s", cmd->hint);
    } else if (cmd->argtable) {
        /* Generate hint based on cmd->argtable */
        arg_dstr_t ds = arg_dstr_create();
        arg_print_syntax_ds(ds, cmd->argtable, NULL);
        item->hint = strdup(arg_dstr_cstr(ds));
        arg_dstr_destroy(ds);
    }
    item->argtable = cmd->argtable;

    if (cmd->func) {
        item->func = cmd->func;
    } else {
        // cmd->func_w_context is valid here according to check above
        item->func_w_context = cmd->func_w_context;
        item->context = cmd->context;
    }

    cmd_item_t *last;
    cmd_item_t *it;
#if CONFIG_CONSOLE_SORTED_HELP
    last = NULL;
    SLIST_FOREACH(it, &s_cmd_list, next) {
        if (strcmp(it->command, item->command) > 0) {
            break;
        }
        last = it;
    }
#else
    last = SLIST_FIRST(&s_cmd_list);
#endif
    if (last == NULL) {
        SLIST_INSERT_HEAD(&s_cmd_list, item, next);
    } else {
#if !CONFIG_CONSOLE_SORTED_HELP
        while ((it = SLIST_NEXT(last, next)) != NULL) {
            last = it;
        }
#endif
        SLIST_INSERT_AFTER(last, item, next);
    }
    return 0;
}

void esp_console_get_completion(const char *buf, linenoiseCompletions *lc)
{
    size_t len = strlen(buf);
    if (len == 0) {
        return;
    }
    cmd_item_t *it;
    SLIST_FOREACH(it, &s_cmd_list, next) {
        /* Check if command starts with buf */
        if (strncmp(buf, it->command, len) == 0) {
            linenoiseAddCompletion(lc, it->command);
        }
    }
}

const char *esp_console_get_hint(const char *buf, int *color, int *bold)
{
    size_t len = strlen(buf);
    cmd_item_t *it;
    SLIST_FOREACH(it, &s_cmd_list, next) {
        if (strlen(it->command) == len &&
                strncmp(buf, it->command, len) == 0) {
            *color = s_config.hint_color;
            *bold = s_config.hint_bold;
            return it->hint;
        }
    }
    return NULL;
}

static const cmd_item_t *find_command_by_name(const char *name)
{
    const cmd_item_t *cmd = NULL;
    cmd_item_t *it;
    size_t len = strlen(name);
    SLIST_FOREACH(it, &s_cmd_list, next) {
        if (strlen(it->command) == len &&
                strcmp(name, it->command) == 0) {
            cmd = it;
            break;
        }
    }
    return cmd;
}

int esp_console_run(const char *cmdline, int *cmd_ret)
{
    if (s_tmp_line_buf == NULL) {
        return -1;
    }
    char **argv = (char **)calloc(s_config.max_cmdline_args, sizeof(char *));
    if (argv == NULL) {
        return -2;
    }
    strlcpy(s_tmp_line_buf, cmdline, s_config.max_cmdline_length);

    size_t argc = esp_console_split_argv(s_tmp_line_buf, argv,
                                         s_config.max_cmdline_args);
    if (argc == 0) {
        free(argv);
        return -1;
    }
    const cmd_item_t *cmd = find_command_by_name(argv[0]);
    if (cmd == NULL) {
        free(argv);
        return -3;
    }
    if (cmd->func) {
        *cmd_ret = (*cmd->func)(argc, argv);
    }
    if (cmd->func_w_context) {
        *cmd_ret = (*cmd->func_w_context)(cmd->context, argc, argv);
    }
    free(argv);
    return 0;
}

static struct {
    struct arg_str *help_cmd;
    struct arg_int *verbose_level;
    struct arg_end *end;
} help_args;

static void print_arg_help(cmd_item_t *it)
{
    /* First line: command name and hint
     * Pad all the hints to the same column
     */
    const char *hint = (it->hint) ? it->hint : "";
    printf("%-s %s\n", it->command, hint);
    /* Second line: print help.
     * Argtable has a nice helper function for this which does line
     * wrapping.
     */
    printf("  "); // arg_print_formatted does not indent the first line
    arg_print_formatted(stdout, 2, 78, it->help);
    /* Finally, print the list of arguments */
    if (it->argtable) {
        arg_print_glossary(stdout, (void **) it->argtable, "  %12s  %s\n");
    }
    printf("\n");
}

static void print_arg_command(cmd_item_t *it)
{
    const char *hint = (it->hint) ? it->hint : "";
    printf("%-s %s\n\n", it->command, hint);
}

static fn_print_arg_t print_verbose_level_arr[ESP_CONSOLE_HELP_VERBOSE_LEVEL_MAX_NUM] = {
    print_arg_command,
    print_arg_help,
};

static int help_command(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &help_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, help_args.end, argv[0]);
        return 1;
    }

    cmd_item_t *it;
    int ret_value = 1;
    esp_console_help_verbose_level_e verbose_level;

    if (help_args.help_cmd->count == 0) {
        /* If verbose level is not provided, set it as default value */
        if (help_args.verbose_level->count == 0) {
            verbose_level = s_verbose_level;
        }
        /* Else use the verbose level from input argument */
        else {
            verbose_level = (esp_console_help_verbose_level_e)help_args.verbose_level->ival[0];
        }
        /* Check if selected verbose level is valid, proceed if yes, else return */
        if (verbose_level >= ESP_CONSOLE_HELP_VERBOSE_LEVEL_MAX_NUM) {
            printf("help: invalid verbose level %d", (int)verbose_level);
            return 1;
        }

        /* Print info of each command based on verbose level */
        SLIST_FOREACH(it, &s_cmd_list, next) {
            if (it->help == NULL) {
                continue;
            }
            print_verbose_level_arr[verbose_level](it);
        }
        ret_value = 0;
    } else {
        /* Print summary of given command, verbose option will be ignored */
        bool found_command = false;
        SLIST_FOREACH(it, &s_cmd_list, next) {
            if (it->help == NULL) {
                continue;
            }
            if (strcmp(help_args.help_cmd->sval[0], it->command) == 0) {
                print_arg_help(it);
                found_command = true;
                ret_value = 0;
                break;
            }
        }

        /* If given command has not been found, print error message*/
        if (!found_command) {
            printf("help: Unrecognized option '%s'. Please use correct command as argument "
                   "or type 'help' only to print help for all commands\n", help_args.help_cmd->sval[0]);
        }
    }

    return ret_value;
}

int esp_console_register_help_command(void)
{
    help_args.help_cmd = arg_str0(NULL, NULL, "<string>", "Name of command");
    help_args.verbose_level = arg_intn("v", "verbose", "<0|1>", 0, 1,
                                       "If specified, list console commands with given verbose level");
    help_args.end = arg_end(2);

    esp_console_cmd_t command = {
        .command = "help",
        .help = "Print the summary of all registered commands if no arguments "
        "are given, otherwise print summary of given command.",
        .func = &help_command,
        .argtable = &help_args
    };
    return esp_console_cmd_register(&command);
}

// static int hello_cmd(int argc, char **argv)
// {
//     printf("Hello, world!\n");
//     return 0;
// }

// void register_system_commands(void)
// {
//     const esp_console_cmd_t cmd = {
//         .command = "hello",
//         .help = "Print hello world",
//         .hint = NULL,
//         .func = &hello_cmd,
//     };
//     esp_console_cmd_register(&cmd);
// }
int esp_console_set_help_verbose_level(esp_console_help_verbose_level_e verbose_level)
{
    /* legal verbose level sanity check */
    if (verbose_level >= ESP_CONSOLE_HELP_VERBOSE_LEVEL_MAX_NUM) {
        return -1;
    }
    s_verbose_level = verbose_level;
    return 0;
}
