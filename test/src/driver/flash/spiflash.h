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

#ifndef __SPIFLASH_H_
#define __SPIFLASH_H_

#include "xil_types.h"
#include <stdbool.h>

#define SPI_FLASH_SECTOR_SIZE (64 * 1024) 
#define SPI_FLASH_PAGE_SIZE 256
#define SPI_FLASH_CHIP_SIZE (1024 * 1024 * 32) 

#define QSPI_PARTITION_LFS_OFFSET 0x00100000 //
#define QSPI_PARTITION_LFS_SIZE 0x00100000 // 1MB
#define FLASH_READ_WRITE_RATE 100 // 1 Mbps

typedef struct spiflash_dev spiflash_handle_t;

void spi_flash_init(void);
spiflash_handle_t *spiflash_get_handle(int dev_id);
int flash_4B_addr_enable(spiflash_handle_t *flash, bool enable);
int flash_erase_sector(spiflash_handle_t *flash, uint32_t addr);
int flash_erase_block64K(spiflash_handle_t *flash, uint32_t addr);
int flash_erase_chip(spiflash_handle_t *flash);
int flash_soft_reset(spiflash_handle_t *flash);
int flash_read_id(spiflash_handle_t *flash, uint8_t id_buf[2]);
uint32_t flash_page_program(spiflash_handle_t *flash, uint32_t addr, uint8_t *data, uint32_t size, uint32_t timeout_ms);
uint32_t flash_read_data(spiflash_handle_t *flash, uint32_t addr, uint8_t *data, uint32_t size, uint32_t timeout_ms);
uint32_t flash_fast_read_data(spiflash_handle_t *flash, uint32_t addr, uint8_t *data, uint32_t size, uint32_t timeout_ms);
void register_sf_commands(void);
#endif /* __SPIFLASH_H_ */
