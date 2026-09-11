#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "test_case.h"
#include "cJSON.h"
#include "shell.h"

static int test_case_init_cmd(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: test_case init <config_file> <md5>\n");
        return -1;
    }
    
    int ret = test_case_init(argv[1], argv[2]);
    if (ret == 0) {
        printf("Test case initialized successfully with config file: %s\n", argv[1]);
    } else {
        printf("Failed to initialize test case\n");
    }
    return ret;
}

static int test_case_start_cmd(int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: test_case start\n");
        return -1;
    }
    
    int ret = test_case_start();
    if (ret == 0) {
        printf("Test started\n");
    } else {
        printf("Failed to start test\n");
    }
    return ret;
}

static int test_case_stop_cmd(int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: test_case stop\n");
        return -1;
    }
    
    int ret = test_case_stop();
    if (ret == 0) {
        printf("Test stopped\n");
    } else {
        printf("Failed to stop test\n");
    }
    return ret;
}

static int test_case_pause_cmd(int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: test_case pause\n");
        return -1;
    }
    
    int ret = test_case_pause();
    if (ret == 0) {
        printf("Test paused\n");
    } else {
        printf("Failed to pause test\n");
    }
    return ret;
}

static int test_case_resume_cmd(int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: test_case resume\n");
        return -1;
    }
    
    int ret = test_case_resume();
    if (ret == 0) {
        printf("Test resumed\n");
    } else {
        printf("Failed to resume test\n");
    }
    return ret;
}

static int test_case_info_cmd(int argc, char **argv)
{
    if (argc != 1) {
        printf("Usage: test_case info\n");
        return -1;
    }

    char *json_str = test_case_get_info_str();
    if (json_str)
    {
        printf("%s\n", json_str);
        cJSON_free(json_str); // 释放内存
    }

    return 0;
}

static int test_case_cmd(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: test_case <command> [args]\n");
        printf("Commands:\n");
        printf("  init <config_file> <md5> - Initialize test case with config file and MD5\n");
        printf("  start             - Start test\n");
        printf("  stop              - Stop test\n");
        printf("  pause             - Pause test\n");
        printf("  resume            - Resume test\n");
        printf("  info             - Show test info\n");
        return -1;
    }
    
    if (strcmp(argv[1], "init") == 0) {
        return test_case_init_cmd(argc - 1, &argv[1]);
    } else if (strcmp(argv[1], "start") == 0) {
        return test_case_start_cmd(argc - 1, &argv[1]);
    } else if (strcmp(argv[1], "stop") == 0) {
        return test_case_stop_cmd(argc - 1, &argv[1]);
    } else if (strcmp(argv[1], "pause") == 0) {
        return test_case_pause_cmd(argc - 1, &argv[1]);
    } else if (strcmp(argv[1], "resume") == 0) {
        return test_case_resume_cmd(argc - 1, &argv[1]);
    } else if (strcmp(argv[1], "info") == 0) {
        return test_case_info_cmd(argc - 1, &argv[1]);
    } else {
        printf("Unknown command: %s\n", argv[1]);
        return -1;
    }
}

void register_test_case_commands(void)
{
    const esp_console_cmd_t cmd = {
        .command = "test_case",
        .help = "Test case management commands",
        .hint = NULL,
        .func = &test_case_cmd,
    };
    esp_console_cmd_register(&cmd);
}
