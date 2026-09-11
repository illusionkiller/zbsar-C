
#include <shell.h>
#include "../../../test/src/env/env.h"

/**
 * @brief Console command "getenv" implementation (U-Boot style)
 * Get the value of an environment variable
 */
static int getenv_cmd(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: getenv <key>\n");
        return 1;
    }
    
    const char *value = getenv(argv[1]);
    if (!value) {
        printf("Error: Environment variable not found: %s\n", argv[1]);
        return 1;
    }
    
    printf("%s\n", value);
    return 0;
}

/**
 * @brief Console command "setenv" implementation (U-Boot style)
 * Set the value of an environment variable
 */
static int setenv_cmd(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: setenv <key> <value>\n");
        return 1;
    }
    
    if (setenv(argv[1], argv[2], 1) != 0) {
        printf("Error: Failed to set environment variable %s\n", argv[1]);
        return 1;
    }
    
    printf("setenv %s=%s\n", argv[1], argv[2]);
    return 0;
}

/**
 * @brief Console command "delenv" implementation (U-Boot style)
 * Delete an environment variable
 */
static int unsetenv_cmd(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: unsetenv <key>\n");
        return 1;
    }
    
    if (unsetenv(argv[1]) != 0) {
        printf("Error: Failed to delete environment variable %s\n", argv[1]);
        return 1;
    }
    
    printf("Deleted environment variable: %s\n", argv[1]);
    return 0;
}

/**
 * @brief Console command "saveenv" implementation (U-Boot style)
 * Save environment variables to LFS
 */
static int saveenv_cmd(int argc, char **argv)
{
    env_save();
    return 0;
}

/**
 * @brief Console command "loadenv" implementation (U-Boot style)
 * Load environment variables from LFS
 */
static int loadenv_cmd(int argc, char **argv)
{
    env_load();
    return 0;
}

/**
 * @brief Console command "printenv" implementation (U-Boot style)
 * Print all environment variables
 */
static int printenv_cmd(int argc, char **argv)
{
    return env_print();
}

// 注册getenv命令 (U-Boot style)
void register_getenv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "getenv",
        .help = "Get environment variable value",
        .hint = "<key>",
        .func = &getenv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

// 注册setenv命令 (U-Boot style)
void register_setenv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "setenv",
        .help = "Set environment variable value",
        .hint = "<key> <value>",
        .func = &setenv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

// 注册unsetenv命令 (U-Boot style)
void register_unsetenv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "unsetenv",
        .help = "Delete environment variable",
        .hint = "<key>",
        .func = &unsetenv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

// 注册saveenv命令 (U-Boot style)
void register_saveenv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "saveenv",
        .help = "Save environment variables to LFS",
        .hint = NULL,
        .func = &saveenv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

// 注册loadenv命令 (U-Boot style)
void register_loadenv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "loadenv",
        .help = "Load environment variables from LFS",
        .hint = NULL,
        .func = &loadenv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

/**
 * @brief Console command "cleanenv" implementation (U-Boot style)
 * Clean all environment variables and reset to defaults
 */
static int cleanenv_cmd(int argc, char **argv)
{
    return env_clean();
}

// 注册printenv命令 (U-Boot style)
void register_printenv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "printenv",
        .help = "Print all environment variables",
        .hint = NULL,
        .func = &printenv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

// 注册cleanenv命令 (U-Boot style)
void register_cleanenv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "cleanenv",
        .help = "Clean all environment variables and reset to defaults",
        .hint = NULL,
        .func = &cleanenv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

/**
 * @brief Register all environment variable commands (U-Boot style)
 */
void register_env_commands(void)
{
    register_getenv_cmd();
    register_setenv_cmd();
    register_unsetenv_cmd();
    register_saveenv_cmd();
    register_loadenv_cmd();
    register_printenv_cmd();
    register_cleanenv_cmd();
}
