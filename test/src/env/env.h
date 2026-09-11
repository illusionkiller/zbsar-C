/*
 * Copyright (C) 2017 - 2022 Xilinx, Inc.
 * Copyright (C) 2022 - 2023 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#ifndef __ENV_H_
#define __ENV_H_
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#define FIRMWARE_VERSION "release_v1.4"
#define DEVICE_SN "PLDIUDJ-2607001"
#define TEST_PARAM_FILE_NAME "test_params"
#define TEST_CASE_FILE_NAME "test_case"
// 环境变量文件路径
#define ENV_FILE_PATH "/env.txt"

//dont
#define DEFAULT_ENV_TABLE() \
{ \
    {"IP", "192.168.0.10"}, \
    {"mask", "255.255.255.0"}, \
    {"gateway", "192.168.0.1"}, \
    {"mac", "00:0A:35:00:01:02"}, \
    {"ntpserver", "192.168.1.215"} \
}


// 环境变量迭代器结构
typedef struct {
    char **current;
} env_iterator_t;

// 环境变量迭代器API
void env_iter_init(env_iterator_t *iter);
int env_iter_next(env_iterator_t *iter, char **key, size_t *key_len, char **value, size_t *value_len);
char *env_iter_next_full(env_iterator_t *iter);

// 环境变量管理API
int env_init(void);
int env_save(void);
int env_load(void);
int env_print(void);
int env_clean(void);
int env_restore_default(void);

#endif
