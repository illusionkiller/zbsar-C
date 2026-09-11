/*-
 * BSD 2-Clause License
 *
 * Copyright (c) 2012-2018, Jan Breuer
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file   scpi-def.c
 * @date   Thu Nov 15 10:58:45 UTC 2012
 *
 * @brief  SCPI parser test
 *
 *
 */

#include <stdlib.h>
#include "scpi-def.h"
#include "vfs.h"
#include "pwmdev.h"
#include "pin.h"
#include "cJSON.h"
#include "bnc_channel.h"
#include "sync_rs422tx_channel.h"
#include "sync_rs422rx_channel.h"
#include "test_case.h"

static scpi_result_t DMM_MeasureVoltageDcQ(scpi_t * context) {
    scpi_number_t param1, param2;
    char bf[15];
    fprintf(stderr, "meas:volt:dc\r\n"); /* debug command name */

    /* read first parameter if present */
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param1, FALSE)) {
        /* do something, if parameter not present */
    }

    /* read second paraeter if present */
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param2, FALSE)) {
        /* do something, if parameter not present */
    }


    SCPI_NumberToStr(context, scpi_special_numbers_def, &param1, bf, 15);
    fprintf(stderr, "\tP1=%s\r\n", bf);


    SCPI_NumberToStr(context, scpi_special_numbers_def, &param2, bf, 15);
    fprintf(stderr, "\tP2=%s\r\n", bf);

    SCPI_ResultDouble(context, 0);

    return SCPI_RES_OK;
}

static scpi_result_t DMM_MeasureVoltageAcQ(scpi_t * context) {
    scpi_number_t param1, param2;
    char bf[15];
    fprintf(stderr, "meas:volt:ac\r\n"); /* debug command name */

    /* read first parameter if present */
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param1, FALSE)) {
        /* do something, if parameter not present */
    }

    /* read second paraeter if present */
    if (!SCPI_ParamNumber(context, scpi_special_numbers_def, &param2, FALSE)) {
        /* do something, if parameter not present */
    }


    SCPI_NumberToStr(context, scpi_special_numbers_def, &param1, bf, 15);
    fprintf(stderr, "\tP1=%s\r\n", bf);


    SCPI_NumberToStr(context, scpi_special_numbers_def, &param2, bf, 15);
    fprintf(stderr, "\tP2=%s\r\n", bf);

    SCPI_ResultDouble(context, 0);

    return SCPI_RES_OK;
}

static scpi_result_t DMM_ConfigureVoltageDc(scpi_t * context) {
    double param1, param2;
    fprintf(stderr, "conf:volt:dc\r\n"); /* debug command name */

    /* read first parameter if present */
    if (!SCPI_ParamDouble(context, &param1, TRUE)) {
        return SCPI_RES_ERR;
    }

    /* read second paraeter if present */
    if (!SCPI_ParamDouble(context, &param2, FALSE)) {
        /* do something, if parameter not present */
    }

    fprintf(stderr, "\tP1=%lf\r\n", param1);
    fprintf(stderr, "\tP2=%lf\r\n", param2);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_Bool(scpi_t * context) {
    scpi_bool_t param1;
    fprintf(stderr, "TEST:BOOL\r\n"); /* debug command name */

    /* read first parameter if present */
    if (!SCPI_ParamBool(context, &param1, TRUE)) {
        return SCPI_RES_ERR;
    }

    fprintf(stderr, "\tP1=%d\r\n", param1);

    return SCPI_RES_OK;
}

scpi_choice_def_t trigger_source[] = {
    {"BUS", 5},
    {"IMMediate", 6},
    {"EXTernal", 7},
    SCPI_CHOICE_LIST_END /* termination of option list */
};

static scpi_result_t TEST_ChoiceQ(scpi_t * context) {

    int32_t param;
    const char * name;

    if (!SCPI_ParamChoice(context, trigger_source, &param, TRUE)) {
        return SCPI_RES_ERR;
    }

    SCPI_ChoiceToName(trigger_source, param, &name);
    fprintf(stderr, "\tP1=%s (%ld)\r\n", name, (long int) param);

    SCPI_ResultInt32(context, param);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_Numbers(scpi_t * context) {
    int32_t numbers[2];

    SCPI_CommandNumbers(context, numbers, 2, 1);

    fprintf(stderr, "TEST numbers %ld %ld\r\n", numbers[0], numbers[1]);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_Text(scpi_t * context) {
    char buffer[100];
    size_t copy_len;

    if (!SCPI_ParamCopyText(context, buffer, sizeof (buffer), &copy_len, FALSE)) {
        buffer[0] = '\0';
    }

    fprintf(stderr, "TEXT: ***%s***\r\n", buffer);

    return SCPI_RES_OK;
}

static scpi_result_t TEST_ArbQ(scpi_t * context) {
    const char * data;
    size_t len;

    if (SCPI_ParamArbitraryBlock(context, &data, &len, FALSE)) {
        SCPI_ResultArbitraryBlock(context, data, len);
    }

    return SCPI_RES_OK;
}

struct _scpi_channel_value_t {
    int32_t row;
    int32_t col;
};
typedef struct _scpi_channel_value_t scpi_channel_value_t;

/**
 * @brief
 * parses lists
 * channel numbers > 0.
 * no checks yet.
 * valid: (@1), (@3!1:1!3), ...
 * (@1!1:3!2) would be 1!1, 1!2, 2!1, 2!2, 3!1, 3!2.
 * (@3!1:1!3) would be 3!1, 3!2, 3!3, 2!1, 2!2, 2!3, ... 1!3.
 *
 * @param channel_list channel list, compare to SCPI99 Vol 1 Ch. 8.3.2
 */
static scpi_result_t TEST_Chanlst(scpi_t *context) {
    scpi_parameter_t channel_list_param;
#define MAXROW 2    /* maximum number of rows */
#define MAXCOL 6    /* maximum number of columns */
#define MAXDIM 2    /* maximum number of dimensions */
    scpi_channel_value_t array[MAXROW * MAXCOL]; /* array which holds values in order (2D) */
    size_t chanlst_idx; /* index for channel list */
    size_t arr_idx = 0; /* index for array */
    size_t n, m = 1; /* counters for row (n) and columns (m) */

    /* get channel list */
    if (SCPI_Parameter(context, &channel_list_param, TRUE)) {
        scpi_bool_t is_range;
        int32_t values_from[MAXDIM];
        int32_t values_to[MAXDIM];
        size_t dimensions;

        bool for_stop_row = FALSE; /* true if iteration for rows has to stop */
        bool for_stop_col = FALSE; /* true if iteration for columns has to stop */
        int32_t dir_row = 1; /* direction of counter for rows, +/-1 */
        int32_t dir_col = 1; /* direction of counter for columns, +/-1 */

        /* the next statement is valid usage and it gets only real number of dimensions for the first item (index 0) */
        if (!SCPI_ExprChannelListEntry(context, &channel_list_param, 0, &is_range, NULL, NULL, 0, &dimensions)) {
            chanlst_idx = 0; /* call first index */
            arr_idx = 0; /* set arr_idx to 0 */
            do { /* if valid, iterate over channel_list_param index while res == valid (do-while cause we have to do it once) */
                SCPI_ExprChannelListEntry(context, &channel_list_param, chanlst_idx, &is_range, values_from, values_to, 4, &dimensions);
                if (is_range == FALSE) { /* still can have multiple dimensions */
                    if (dimensions == 1) {
                        /* here we have our values
                         * row == values_from[0]
                         * col == 0 (fixed number)
                         * call a function or something */
                        array[arr_idx].row = values_from[0];
                        array[arr_idx].col = 0;
                    } else if (dimensions == 2) {
                        /* here we have our values
                         * row == values_fom[0]
                         * col == values_from[1]
                         * call a function or something */
                        array[arr_idx].row = values_from[0];
                        array[arr_idx].col = values_from[1];
                    } else {
                        return SCPI_RES_ERR;
                    }
                    arr_idx++; /* inkrement array where we want to save our values to, not neccessary otherwise */
                    if (arr_idx >= MAXROW * MAXCOL) {
                        return SCPI_RES_ERR;
                    }
                } else if (is_range == TRUE) {
                    if (values_from[0] > values_to[0]) {
                        dir_row = -1; /* we have to decrement from values_from */
                    } else { /* if (values_from[0] < values_to[0]) */
                        dir_row = +1; /* default, we increment from values_from */
                    }

                    /* iterating over rows, do it once -> set for_stop_row = false
                     * needed if there is channel list index isn't at end yet */
                    for_stop_row = FALSE;
                    for (n = values_from[0]; for_stop_row == FALSE; n += dir_row) {
                        /* usual case for ranges, 2 dimensions */
                        if (dimensions == 2) {
                            if (values_from[1] > values_to[1]) {
                                dir_col = -1;
                            } else if (values_from[1] < values_to[1]) {
                                dir_col = +1;
                            }
                            /* iterating over columns, do it at least once -> set for_stop_col = false
                             * needed if there is channel list index isn't at end yet */
                            for_stop_col = FALSE;
                            for (m = values_from[1]; for_stop_col == FALSE; m += dir_col) {
                                /* here we have our values
                                 * row == n
                                 * col == m
                                 * call a function or something */
                                array[arr_idx].row = n;
                                array[arr_idx].col = m;
                                arr_idx++;
                                if (arr_idx >= MAXROW * MAXCOL) {
                                    return SCPI_RES_ERR;
                                }
                                if (m == (size_t)values_to[1]) {
                                    /* endpoint reached, stop column for-loop */
                                    for_stop_col = TRUE;
                                }
                            }
                            /* special case for range, example: (@2!1) */
                        } else if (dimensions == 1) {
                            /* here we have values
                             * row == n
                             * col == 0 (fixed number)
                             * call function or sth. */
                            array[arr_idx].row = n;
                            array[arr_idx].col = 0;
                            arr_idx++;
                            if (arr_idx >= MAXROW * MAXCOL) {
                                return SCPI_RES_ERR;
                            }
                        }
                        if (n == (size_t)values_to[0]) {
                            /* endpoint reached, stop row for-loop */
                            for_stop_row = TRUE;
                        }
                    }


                } else {
                    return SCPI_RES_ERR;
                }
                /* increase index */
                chanlst_idx++;
            } while (SCPI_EXPR_OK == SCPI_ExprChannelListEntry(context, &channel_list_param, chanlst_idx, &is_range, values_from, values_to, 4, &dimensions));
            /* while checks, whether incremented index is valid */
        }
        /* do something at the end if needed */
        /* array[arr_idx].row = 0; */
        /* array[arr_idx].col = 0; */
    }

    {
        size_t i;
        fprintf(stderr, "TEST_Chanlst: ");
        for (i = 0; i< arr_idx; i++) {
            fprintf(stderr, "%ld!%ld, ", array[i].row, array[i].col);
        }
        fprintf(stderr, "\r\n");
    }
    return SCPI_RES_OK;
}

/**
 * Reimplement IEEE488.2 *TST?
 *
 * Result should be 0 if everything is ok
 * Result should be 1 if something goes wrong
 *
 * Return SCPI_RES_OK
 */
static scpi_result_t My_CoreTstQ(scpi_t * context) {

    SCPI_ResultInt32(context, 0);

    return SCPI_RES_OK;
}

// 设置脉冲通道宽度
static scpi_result_t PULM_OC_Channel_Width(scpi_t *context)
{
    uint32_t channel_id; // 通道ID
    uint32_t width_ms;   // 脉冲宽度(毫秒)
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    fprintf(stderr, "PULM:OC#:WIDTH\r\n"); /* debug command name */
    channel_id = numbers[0];
    //验证通道ID范围(0 - 63)
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    if (!SCPI_ParamUInt32(context, &width_ms, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    u32 width_us = (u32)(width_ms * 1000)-1; 

    if(width_us > PWM_CHANNEL_MAX_CLOCK)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    fprintf(stderr, "\tChannel=%ld, Width=%lu ms (%lu us)\r\n",
            (long int)channel_id, width_ms, width_us);

    int dev_id = pwm_dev_open("oc", 0);
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    pwm_data.value = width_us;
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_DUTY, &pwm_data);

    return SCPI_RES_OK;
}

// 查询脉冲通道宽度
static scpi_result_t PULM_OC_Channel_WidthQ(scpi_t *context)
{
    int32_t channel_id; // 通道ID
    int32_t numbers[1];
    fprintf(stderr, "PULM:OC#:WIDTH?\r\n"); /* debug command name */

    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
    // 验证通道ID范围(0 - 63)
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }

     u32 width_us = 0;
     pwm_dev_data_t pwm_data = {0};
     pwm_data.channel = channel_id;
     int dev_id = pwm_dev_open("oc", 0);
     pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_GET_DUTY, &pwm_data);
     width_us = pwm_data.value;
     u32 width_ms = (width_us+1)/1000; // 转换为毫秒

     fprintf(stderr, "\tChannel=%ld, Width=%ld ms\r\n",
             (long int)channel_id, width_ms);

     // 返回脉冲宽度(毫秒)
     SCPI_ResultInt32(context, width_ms);

     return SCPI_RES_OK;
}

static scpi_result_t PULM_OC_Trigger(scpi_t *context)
{
    fprintf(stderr, "PULM:OC:TRG\r\n"); /* debug command name */
    int32_t numbers[1];

    SCPI_CommandNumbers(context, numbers, 1, 1);
    int32_t channel_id = numbers[0];
    // 验证通道ID范围(0 - 63)
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    // 触发通道
    int dev_id = pwm_dev_open("oc", 0);
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
	pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ONE_PULSE_MODE, &pwm_data);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ENABLE, &pwm_data);
    fprintf(stderr, "\tChannel=%ld, Trigger\r\n", (long int)channel_id);

    return SCPI_RES_OK;
}

static scpi_result_t PULM_OE_Channel_Width(scpi_t *context)
{
    uint32_t channel_id; // 通道ID
    uint32_t width_ms;   // 脉冲宽度(毫秒)
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    fprintf(stderr, "PULM:OE#:WIDTH\r\n"); /* debug command name */
    channel_id = numbers[0];
    // 验证通道ID范围
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    if (!SCPI_ParamUInt32(context, &width_ms, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    u32 width_us = width_ms > 0 ? (u32)(width_ms * 1000) - 1 : 0;

    if (width_us > PWM_CHANNEL_MAX_CLOCK)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    fprintf(stderr, "\tChannel=%ld, Width=%lu ms (%lu us)\r\n",
            (long int)channel_id, width_ms, width_us);

    int dev_id = pwm_dev_open("oe", 0);
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    pwm_data.value = width_us;
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_DUTY, &pwm_data);

    return SCPI_RES_OK;
}

// 查询脉冲通道宽度
static scpi_result_t PULM_OE_Channel_WidthQ(scpi_t *context)
{
    int32_t channel_id; // 通道ID
    int32_t numbers[1];
    fprintf(stderr, "PULM:OE#:WIDTH?\r\n"); /* debug command name */

    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
  
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }

    u32 width_us = 0;
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    int dev_id = pwm_dev_open("oe", 0);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_GET_DUTY, &pwm_data);
    width_us = pwm_data.value;
    u32 width_ms = (width_us + 1) / 1000; // 转换为毫秒

    fprintf(stderr, "\tChannel=%ld, Width=%ld ms\r\n",
            (long int)channel_id, width_ms);

    // 返回脉冲宽度(毫秒)
    SCPI_ResultInt32(context, width_ms);

    return SCPI_RES_OK;
}

static scpi_result_t PULM_OE_Trigger(scpi_t *context)
{
    fprintf(stderr, "PULM:OE:TRG\r\n"); /* debug command name */
    int32_t numbers[1];

    SCPI_CommandNumbers(context, numbers, 1, 1);
    int32_t channel_id = numbers[0];
    // 验证通道ID范围(0 - 63)
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    // 触发通道
    int dev_id = pwm_dev_open("oe", 0);
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
	pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ONE_PULSE_MODE, &pwm_data);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ENABLE, &pwm_data);
    fprintf(stderr, "\tChannel=%ld, Trigger\r\n", (long int)channel_id);

    return SCPI_RES_OK;
}

static scpi_result_t PULM_RST_Channel_Width(scpi_t *context)
{
    uint32_t channel_id; // 通道ID
    uint32_t width_ms;   // 脉冲宽度(毫秒)
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    fprintf(stderr, "PULM:RST#:WIDTH\r\n"); /* debug command name */
    channel_id = numbers[0];
    // 验证通道ID范围
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    if (!SCPI_ParamUInt32(context, &width_ms, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    u32 width_us = width_ms > 0 ? (u32)(width_ms * 1000) - 1 : 0;

    if (width_us > PWM_CHANNEL_MAX_CLOCK)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    fprintf(stderr, "\tChannel=%ld, Width=%lu ms (%lu us)\r\n",
            (long int)channel_id, width_ms, width_us);

    int dev_id = pwm_dev_open("rst", 0);
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    pwm_data.value = width_us;
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_DUTY, &pwm_data);

    return SCPI_RES_OK;
}

// 查询脉冲通道宽度
static scpi_result_t PULM_RST_Channel_WidthQ(scpi_t *context)
{
    int32_t channel_id; // 通道ID
    int32_t numbers[1];
    fprintf(stderr, "PULM:RST#:WIDTH?\r\n"); /* debug command name */

    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];

    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }

    u32 width_us = 0;
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    int dev_id = pwm_dev_open("rst", 0);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_GET_DUTY, &pwm_data);
    width_us = pwm_data.value;
    u32 width_ms = (width_us + 1) / 1000; // 转换为毫秒

    fprintf(stderr, "\tChannel=%ld, Width=%ld ms\r\n",
            (long int)channel_id, width_ms);

    // 返回脉冲宽度(毫秒)
    SCPI_ResultInt32(context, width_ms);

    return SCPI_RES_OK;
}

static scpi_result_t PULM_RST_Trigger(scpi_t *context)
{
    fprintf(stderr, "PULM:RST:TRG\r\n"); /* debug command name */
    int32_t numbers[1];

    SCPI_CommandNumbers(context, numbers, 1, 1);
    int32_t channel_id = numbers[0];
    // 验证通道ID范围(0 - 63)
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    // 触发通道
    int dev_id = pwm_dev_open("rst", 0);
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
	pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ONE_PULSE_MODE, &pwm_data);
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ENABLE, &pwm_data);
    fprintf(stderr, "\tChannel=%ld, Trigger\r\n", (long int)channel_id);

    return SCPI_RES_OK;
}
// static scpi_result_t PULM_TriggerMask(scpi_t *context)
// {
//     uint64_t mask;                // 脉冲通道使能掩码
//     fprintf(stderr, "PULM:TRGMASK"); /* debug command name */

//     /* read first parameter if present */
//     if (!SCPI_ParamUInt64(context, &mask, TRUE))
//     {
//         SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
//         return SCPI_RES_ERR;
//     }

//     fprintf(stderr, "\tMask=0x%llx\r\n", mask);
//     int dev_id = pwm_dev_open("pwm_0", 0);
//     pwm_dev_data_t pwm_data = {0};
//     for(uint32_t channel_id = PWM_CHANNEL_START_NUM; channel_id <= PWM_CHANNEL_END_NUM; channel_id++)
//     {
//         uint64_t channel_mask = (1ULL << channel_id);
//         if(mask & channel_mask)
//         {
//             // 触发通道
//             pwm_data.channel = channel_id;
//             pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ENABLE, &pwm_data);
//         }
//     }
//     return SCPI_RES_OK;
// }

// 实现TESTPARA:APPLY命令，用于应用测试参数文件
static scpi_result_t TESTPARA_APPLY(scpi_t *context)
{
    char file_path[256] = {0};
    char file_md5[33] = {0}; // MD5值是32个字符，加上结束符共33个字符长度
    size_t copy_len;
    
    fprintf(stderr, "TESTPARA:APPLY\r\n"); /* debug command name */
    
    // 读取文件路径参数
    if (!SCPI_ParamCopyText(context, file_path, sizeof(file_path), &copy_len, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    
    // 读取文件MD5参数
    if (!SCPI_ParamCopyText(context, file_md5, sizeof(file_md5), &copy_len, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    
    fprintf(stderr, "\tFile path: %s\r\n", file_path);
    fprintf(stderr, "\tFile MD5: %s\r\n", file_md5);

    uint8_t md5[16] = {0};
    char *content = NULL;
    size_t content_size = 0;
    // 文件校验
    if (vfs_read_content_with_md5(file_path, &content, &content_size, md5) != 0)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }
    // 转换为字符串
    char md5_str[33] = {0};
    for(int i = 0; i < 16; i++)
    {
        sprintf(&md5_str[i*2], "%02x", md5[i]);
    }
    if(strcmp(file_md5, md5_str) != 0)
    {
        vPortFree(content);
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }
    vPortFree(content);
    setenv(TEST_PARAM_FILE_NAME, file_path, 1);
    return SCPI_RES_OK;
}

//
static scpi_result_t TESTPARA_APPLY_Q(scpi_t *context)
{
    fprintf(stderr, "TESTPARA:APPLY?\r\n"); /* debug command name */
    
    char *file_path = getenv(TEST_PARAM_FILE_NAME);
    if(file_path == NULL)
    {
        SCPI_ResultText(context, "No file applied");
        return SCPI_RES_OK;
    }
    SCPI_ResultText(context, file_path);
    
    return SCPI_RES_OK;
}

static scpi_result_t TEST_START(scpi_t *context)
{
    char file_path[256] = {0};
    char file_md5[33] = {0};
    size_t copy_len;

    fprintf(stderr, "TEST:START\r\n");
    if (!SCPI_ParamCopyText(context, file_path, sizeof(file_path), &copy_len, TRUE) ||
        !SCPI_ParamCopyText(context, file_md5, sizeof(file_md5), &copy_len, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }

    if (test_case_init(file_path, file_md5) != 0)
    {
        fprintf(stderr, "Failed to initialize test case\r\n");
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }
    if (test_case_start() != 0)
    {
        fprintf(stderr, "Failed to start test case\r\n");
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

static scpi_result_t TEST_INFO_Q(scpi_t *context)
{
//    fprintf(stderr, "TEST:INFO?\r\n"); /* debug command name */
    char *json_str = test_case_get_info_str();
    if (json_str)
    {
    	SCPI_ResultMnemonic(context, json_str);
        cJSON_free(json_str); // 释放内存
    }
    else
    {
    	SCPI_ResultMnemonic(context, "");
    }

    return SCPI_RES_OK;
}


static scpi_result_t TEST_PAUSE(scpi_t *context)
{
    fprintf(stderr, "TEST:PAUSE\r\n"); /* debug command name */
    test_case_pause();
    return SCPI_RES_OK;
}


static scpi_result_t TEST_RESUME(scpi_t *context)
{
    fprintf(stderr, "TEST:RESUME\r\n"); /* debug command name */
    test_case_resume();
    return SCPI_RES_OK;
}

static scpi_result_t TEST_STOP(scpi_t *context)
{
    fprintf(stderr, "TEST:STOP\r\n"); /* debug command name */
    // char *file_path = getenv(TEST_CASE_FILE_NAME);
    // if (file_path == NULL)
    // {
    //     SCPI_ResultText(context, "No file applied");
    //     return SCPI_RES_OK;
    // }
    test_case_stop();
//    fprintf(stderr, "stop test case %s\r\n", file_path);
    return SCPI_RES_OK;
}

static scpi_result_t FAN_Channel_Ctrl(scpi_t *context)
{
    uint32_t channel_id; // 通道ID
    uint32_t period_ms;   // 周期(毫秒)
    uint32_t duty_ms;   // 占空比(毫秒)
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    fprintf(stderr, "PULM:FAN#:PERIOD\r\n"); /* debug command name */
    channel_id = numbers[0];
    // 验证通道ID范围
    if (channel_id < PWM_CHANNEL_START_NUM || channel_id > PWM_CHANNEL_END_NUM)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    if (!SCPI_ParamUInt32(context, &period_ms, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    u32 period_us = period_ms > 0 ? (u32)(period_ms * 1000) - 1 : 0;

    if (period_us > PWM_CHANNEL_MAX_CLOCK)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    if (!SCPI_ParamUInt32(context, &duty_ms, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    u32 duty_us = duty_ms > 0 ? (u32)(duty_ms * 1000) - 1 : 0;
    fprintf(stderr, "\tChannel=%ld, Period=%lu ms (%lu us), Duty=%lu ms (%lu us)\r\n",
            (long int)channel_id, period_ms, period_us, duty_ms, duty_us);

    int dev_id = pwm_dev_open("fan", 0);
    pwm_dev_data_t pwm_data = {0};
    pwm_data.channel = channel_id;
    pwm_data.value = period_us;
    pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_DISABLE, &pwm_data);
    if (duty_ms > 0 && duty_ms < period_ms)
    {
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_PERIOD, &pwm_data);
        pwm_data.value = duty_us;
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_SET_DUTY, &pwm_data);
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_CONTINUOUS_WAVE_MODE, &pwm_data);
        pwm_dev_ioctl(dev_id, PWM_DEV_IOCTL_ENABLE, &pwm_data);
    }
    return SCPI_RES_OK;
}

static scpi_result_t SYNCRS422TX_Channel_Stat(scpi_t *context)
{
    int32_t channel_id;
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
    void *channel = sync_rs422tx_channel_get_handle(channel_id);
    if(channel == NULL)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    bool value = 0;
    if (!SCPI_ParamBool(context, &value, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    fprintf(stderr, "SYNCRS422ch%ld:STATe %d\r\n", channel_id, value); /* debug command name */
    int ret = 0;
    if(value)
    {
        ret = sync_rs422tx_channel_open(channel);
        if(ret != 0)
        {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
    }
    else
    {
        sync_rs422tx_channel_close(channel);
    }

    return SCPI_RES_OK;
}

static scpi_result_t SYNCRS422TX_Channel_StatQ(scpi_t *context)
{

    int32_t channel_id;                                //
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
    // fprintf(stderr, "SYNCRS422ch%ld:CHANnel:STATe?\r\n", channel_id); /* debug command name */
    void *channel = sync_rs422tx_channel_get_handle(channel_id);
    if(channel == NULL)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    bool value = sync_rs422tx_channel_is_opened(channel);
    SCPI_ResultBool(context, value);
    return SCPI_RES_OK;
}

static scpi_result_t SYNCRS422RX_Channel_Stat(scpi_t *context)
{
    int32_t channel_id;
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
	void *channel = sync_rs422rx_channel_get_handle(channel_id);
	if (channel == NULL)
	{
	 SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
	 return SCPI_RES_ERR;
	}
    bool value = 0;
    if (!SCPI_ParamBool(context, &value, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    // fprintf(stderr, "SYNCRS422RXch%ld:STATe %d\r\n", channel_id, value); /* debug command name */
    int ret = 0;
     if (value)
     {
         ret = sync_rs422rx_channel_open(channel);
         if (ret != 0)
         {
             SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
             return SCPI_RES_ERR;
         }
     }
     else
     {
         sync_rs422rx_channel_close(channel);
     }

    return SCPI_RES_OK;
}

static scpi_result_t SYNCRS422RX_Channel_StatQ(scpi_t *context)
{

    int32_t channel_id; //
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
    // fprintf(stderr, "SYNCRS422RXch%ld:CHANnel:STATe?\r\n", channel_id); /* debug command name */
    void *channel = sync_rs422rx_channel_get_handle(channel_id);
    if (channel == NULL)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    bool value = sync_rs422rx_channel_is_opened(channel);
    SCPI_ResultBool(context, value);
    return SCPI_RES_OK;
}

static scpi_result_t BNC_Channel_Stat(scpi_t *context)
{
    int32_t channel_id;
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
    void *channel = bnc_channel_get_handle(channel_id);
    if (channel == NULL)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }

    bool value = 0;
    if (!SCPI_ParamBool(context, &value, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }
    fprintf(stderr, "BNCch%ld:STATe %d\r\n", channel_id, value); /* debug command name */

    if (value)
    {
        if (bnc_channel_open(channel) != 0)
        {
            SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
            return SCPI_RES_ERR;
        }
    }
    else
    {
        bnc_channel_close(channel);
    }

    return SCPI_RES_OK;
}

static scpi_result_t BNC_Channel_StatQ(scpi_t *context)
{
    int32_t channel_id;
    int32_t numbers[1];
    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
    // fprintf(stderr, "BNCch%ld:CHANnel:STATe?\r\n", channel_id); /* debug command name */

    void *channel = bnc_channel_get_handle(channel_id);
    if (channel == NULL)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }

    bool value = bnc_channel_is_opened(channel);
    SCPI_ResultBool(context, value);
    return SCPI_RES_OK;
}

static scpi_result_t SYNC_RS422RX_Channel_TriggerQueryData(scpi_t *context)
{
    int32_t channel_id;
    int32_t numbers[1];
    uint32_t num = 0;
    SCPI_CommandNumbers(context, numbers, 1, 1);
    channel_id = numbers[0];
      
    sync_rs422rx_channel_t *channel = sync_rs422rx_channel_get_handle(channel_id);
    if (channel == NULL)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_PARAMETER_NOT_ALLOWED);
        return SCPI_RES_ERR;
    }
    if (!SCPI_ParamUInt32(context, &num, TRUE))
    {
        SCPI_ErrorPush(context, SCPI_ERROR_DATA_TYPE_ERROR);
        return SCPI_RES_ERR;
    }

    int ret = sync_rs422rx_channel_trigger_query_data(channel,num);
    if(ret != 0)
    {
        SCPI_ErrorPush(context, SCPI_ERROR_EXECUTION_ERROR);
        return SCPI_RES_ERR;
    }
    return SCPI_RES_OK;
}

const scpi_command_t scpi_commands[] = {
    /* IEEE Mandated Commands (SCPI std V1999.0 4.1.1) */
    { .pattern = "*CLS", .callback = SCPI_CoreCls,},
    { .pattern = "*ESE", .callback = SCPI_CoreEse,},
    { .pattern = "*ESE?", .callback = SCPI_CoreEseQ,},
    { .pattern = "*ESR?", .callback = SCPI_CoreEsrQ,},
    { .pattern = "*IDN?", .callback = SCPI_CoreIdnQ,},
    { .pattern = "*OPC", .callback = SCPI_CoreOpc,},
    { .pattern = "*OPC?", .callback = SCPI_CoreOpcQ,},
    { .pattern = "*RST", .callback = SCPI_CoreRst,},
    { .pattern = "*SRE", .callback = SCPI_CoreSre,},
    { .pattern = "*SRE?", .callback = SCPI_CoreSreQ,},
    { .pattern = "*STB?", .callback = SCPI_CoreStbQ,},
    { .pattern = "*TST?", .callback = My_CoreTstQ,},
    { .pattern = "*WAI", .callback = SCPI_CoreWai,},

    /* Required SCPI commands (SCPI std V1999.0 4.2.1) */
    {.pattern = "SYSTem:ERRor[:NEXT]?", .callback = SCPI_SystemErrorNextQ,},
    {.pattern = "SYSTem:ERRor:COUNt?", .callback = SCPI_SystemErrorCountQ,},
    {.pattern = "SYSTem:VERSion?", .callback = SCPI_SystemVersionQ,},

    /* {.pattern = "STATus:OPERation?", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:EVENt?", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:CONDition?", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:ENABle", .callback = scpi_stub_callback,}, */
    /* {.pattern = "STATus:OPERation:ENABle?", .callback = scpi_stub_callback,}, */

    {.pattern = "STATus:QUEStionable[:EVENt]?", .callback = SCPI_StatusQuestionableEventQ,},
    /* {.pattern = "STATus:QUEStionable:CONDition?", .callback = scpi_stub_callback,}, */
    {.pattern = "STATus:QUEStionable:ENABle", .callback = SCPI_StatusQuestionableEnable,},
    {.pattern = "STATus:QUEStionable:ENABle?", .callback = SCPI_StatusQuestionableEnableQ,},

    {.pattern = "STATus:PRESet", .callback = SCPI_StatusPreset,},

    /* DMM */
    {.pattern = "MEASure:VOLTage:DC?", .callback = DMM_MeasureVoltageDcQ,},
    {.pattern = "CONFigure:VOLTage:DC", .callback = DMM_ConfigureVoltageDc,},
    {.pattern = "MEASure:VOLTage:DC:RATio?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:VOLTage:AC?", .callback = DMM_MeasureVoltageAcQ,},
    {.pattern = "MEASure:CURRent:DC?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:CURRent:AC?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:RESistance?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:FRESistance?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:FREQuency?", .callback = SCPI_StubQ,},
    {.pattern = "MEASure:PERiod?", .callback = SCPI_StubQ,},

    {.pattern = "SYSTem:COMMunication:TCPIP:CONTROL?", .callback = SCPI_SystemCommTcpipControlQ,},

    {.pattern = "TEST:BOOL", .callback = TEST_Bool,},
    {.pattern = "TEST:CHOice?", .callback = TEST_ChoiceQ,},
    {.pattern = "TEST#:NUMbers#", .callback = TEST_Numbers,},
    {.pattern = "TEST:TEXT", .callback = TEST_Text,},
    {.pattern = "TEST:ARBitrary?", .callback = TEST_ArbQ,},
    {.pattern = "TEST:CHANnellist", .callback = TEST_Chanlst,},
    {.pattern = "PULM:OC#:WIDTh", .callback = PULM_OC_Channel_Width,},
    {.pattern = "PULM:OC#:WIDTh?", .callback = PULM_OC_Channel_WidthQ,},
    {.pattern = "PULM:OE#:WIDTh", .callback = PULM_OE_Channel_Width,},
    {.pattern = "PULM:OE#:WIDTh?", .callback = PULM_OE_Channel_WidthQ,},
    {.pattern = "PULM:RST#:WIDTh", .callback = PULM_RST_Channel_Width,},
    {.pattern = "PULM:RST#:WIDTh?", .callback = PULM_RST_Channel_WidthQ,},
    {.pattern = "PULM:OC#:TRG", .callback = PULM_OC_Trigger,},
    {.pattern = "PULM:OE#:TRG", .callback = PULM_OE_Trigger,},
    {.pattern = "PULM:RST#:TRG", .callback = PULM_RST_Trigger,},
    {.pattern = "TESTPARA:APPLY", .callback = TESTPARA_APPLY,},
    {.pattern = "TESTPARA:APPLY?", .callback = TESTPARA_APPLY_Q,},
    {.pattern = "TEST:START", .callback = TEST_START,},
    {.pattern = "TEST:STOP", .callback = TEST_STOP,},
    {.pattern = "TEST:PAUSE", .callback = TEST_PAUSE,},
    {.pattern = "TEST:RESUME", .callback = TEST_RESUME,},
    {.pattern = "TEST:INFO?", .callback = TEST_INFO_Q,},
    {.pattern = "FAN#:CTRL", .callback = FAN_Channel_Ctrl,},
    {.pattern = "BNCch#:STATe", .callback = BNC_Channel_Stat,},
    {.pattern = "BNCch#:STATe?", .callback = BNC_Channel_StatQ,},
    {.pattern = "SYNCRS422TXch#:STATe", .callback = SYNCRS422TX_Channel_Stat,},
    {.pattern = "SYNCRS422TXch#:STATe?", .callback = SYNCRS422TX_Channel_StatQ,},
    {.pattern = "SYNCRS422RXch#:STATe", .callback = SYNCRS422RX_Channel_Stat,},
    {.pattern = "SYNCRS422RXch#:STATe?", .callback = SYNCRS422RX_Channel_StatQ,},
    {.pattern = "SYNCRS422RXch#:DATA", .callback = SYNC_RS422RX_Channel_TriggerQueryData,},
    SCPI_CMD_LIST_END
};

scpi_interface_t scpi_interface = {
    .error = SCPI_Error,
    .write = SCPI_Write,
    .control = SCPI_Control,
    .flush = SCPI_Flush,
    .reset = SCPI_Reset,
};

char scpi_input_buffer[SCPI_INPUT_BUFFER_LENGTH];
scpi_error_t scpi_error_queue_data[SCPI_ERROR_QUEUE_SIZE];

scpi_t scpi_context;
