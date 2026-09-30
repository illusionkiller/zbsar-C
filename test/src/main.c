/*
 * Copyright (C) 2016 - 2019 Xilinx, Inc.
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

#include <log.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lwip/sys.h"
#include "time.h"
#include "vfs.h"
#include "env.h"

extern void init_cjson_heap_hooks(void);
extern void pin_init(void);
extern int smi_switch_init(void);
extern int network_init(void);
extern void uartlite_init(void);
extern void hwtimer_init(void);
extern void uart16550_init(void);
extern void pwm_dev_init(void);
extern void open_all_fans(void);
extern void sync_rs422rx_channel_init(void);
extern void sync_rs422tx_channel_init(void);
extern void bnc_channel_init(void);

extern int lfs_init(void);
extern int fatfs_init(void);
extern int console_init(void);
extern void sntp_client_init(void);
extern void tftpd_init(void);
extern void system_monitor_start(void);
extern void set_system_time_to_compile_time(void);
extern void scpi_server_init(void);
extern void start_dtu_server(void);

extern int console_init(void);
extern void register_sf_commands(void);
extern void register_env_commands(void);
extern void register_vfs_commands(void);
extern void register_system_commands(void);
extern void register_netif_commands(void);
extern void register_iperf_commands(void);
extern void register_test_case_commands(void);

void main_thread(void *arg)
{
	set_system_time_to_compile_time();
	init_cjson_heap_hooks();
	uartlite_init();
	pin_init();
	fatfs_init();
	lfs_init();
	vfs_init();
	log_init();
	env_init();
//driver
	pwm_dev_init();
	open_all_fans();
	smi_switch_init();
	network_init();
//app channel
	sync_rs422tx_channel_init();
	sync_rs422rx_channel_init();
	bnc_channel_init();
	// wave_channel_init();

//server
	system_monitor_start();
	sntp_client_init();
	tftpd_init();
	scpi_server_init();
	start_dtu_server();

//debug cmd
	register_sf_commands();
	register_env_commands();
	register_vfs_commands();
	register_system_commands();
	register_netif_commands();
	register_iperf_commands();
	register_test_case_commands();
	console_init();
	log_info("FIRMWARE_VERSION: %s build: %s %s ,DEVICE_SN: %s", FIRMWARE_VERSION, __DATE__, __TIME__, DEVICE_SN);
    vTaskDelete(NULL);
}

int main(void)
{
#include "xil_cache.h"
#ifdef __PPC__
	Xil_ICacheEnableRegion(CACHEABLE_REGION_MASK);
	Xil_DCacheEnableRegion(CACHEABLE_REGION_MASK);
#elif __MICROBLAZE__
#ifdef XPAR_MICROBLAZE_USE_ICACHE
	Xil_ICacheEnable();
#endif
#ifdef XPAR_MICROBLAZE_USE_DCACHE
	Xil_DCacheEnable();
#endif
#endif
	sys_thread_new("main_thread", (void (*)(void *))main_thread, NULL,
					   configMINIMAL_STACK_SIZE*2,
					   tskIDLE_PRIORITY + 1);
	vTaskStartScheduler();
	return 0;
}
