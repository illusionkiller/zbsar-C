#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shell.h"
#include "spiflash.h"

// 定义一个结构体来存放所有命令的参数解析表

static struct
{
   struct arg_int *dev;         // sf -d <dev_id>
   struct arg_int *offset;      // sf write <offset>
   struct arg_str *data;        // sf write <offset> <data>
   struct arg_int *size;        // sf read <offset> <size>
                                // sf erase <offset> <size>
   struct arg_str *mode;        // sf <mode>:eraseall benchmark
   struct arg_end *end;
} qspi_flash_args;

void print_flash_hex(uint16_t address, uint8_t *buffer, size_t size)
{
   for (size_t i = 0; i < size; i += 16)
   {
       printf("0x%08X: ", address + i);

       for (size_t j = 0; j < 16; j++)
       {
           if (i + j < size)
           {
               printf("%02X ", buffer[i + j]);
           }
           else
           {
               printf("   ");
           }
       }

       // 显示 ASCII 字符
       for (size_t j = 0; j < 16; j++)
       {
           if (i + j < size)
           {
               uint8_t b = buffer[i + j];
               if (b >= 32 && b < 127)
               {
                   printf("%c", b);
               }
               else
               {
                   printf(".");
               }
           }
           else
           {
               printf(" ");
           }
       }
       printf("\n");
   }
}

static u32 flash_transfer_timeout_ms(u32 size)
{
   return size * 8U / FLASH_READ_WRITE_RATE + 1U;
}

static u32 flash_write(spiflash_handle_t *flash, u32 addr, u8 *data, u32 size)
{
   u32 written = 0;
   while (written < size)
   {
       u32 write_size = SPI_FLASH_PAGE_SIZE - (addr & 0xFF);
       if (write_size > (size - written))
       {
           write_size = size - written;
       }
       u32 ret_size = flash_page_program(flash, addr, &data[written], write_size,
                                         flash_transfer_timeout_ms(write_size));
       if (ret_size == 0)
       {
           break;
       }
       addr += ret_size;
       written += ret_size;
   }
   return written;
}

static u32 flash_read(spiflash_handle_t *flash, u32 addr, u8 *data, u32 size)
{
   u32 read_total = 0;

   while (read_total < size)
   {
      u32 read_size = size - read_total;
      if (read_size > SPI_FLASH_SECTOR_SIZE)
          read_size = SPI_FLASH_SECTOR_SIZE;

      u32 ret_size = flash_fast_read_data(flash, addr + read_total,
                                           &data[read_total], read_size,
                                           flash_transfer_timeout_ms(read_size));
      if (ret_size != read_size)
          break;
      read_total += ret_size;
   }

   return read_total;
}

static int sf_cmd_handler(int argc, char **argv)
{
   int nerrors = arg_parse(argc, argv, (void **)&qspi_flash_args);
   if (nerrors != 0)
   {
       arg_print_errors(stderr, qspi_flash_args.end, argv[0]);
       return 1;
   }
    int dev_id = qspi_flash_args.dev->count ? qspi_flash_args.dev->ival[0] : 0;
    spiflash_handle_t *flash = spiflash_get_handle(dev_id);
    if (flash == NULL)
    {
        printf("invalid flash device id: %d\n", dev_id);
        return 1;
    }
   if (qspi_flash_args.mode->count)
   {
       if (strcmp(qspi_flash_args.mode->sval[0], "eraseall") == 0)
           {
               TickType_t start_time = xTaskGetTickCount();
                int ret = flash_erase_chip(flash);
               TickType_t end_time = xTaskGetTickCount();
               if (ret == 0)
               {
                   printf("erase all flash take %lu ms\n", (end_time - start_time) * portTICK_PERIOD_MS);
               }
               else
               {
                   printf("erase all flash failed\n");
               }
           }
       else if (strcmp(qspi_flash_args.mode->sval[0], "benchmark") == 0)
       {
            const u32 test_size = 1024 * 1024; // 1MB
            const u32 test_addr = QSPI_PARTITION_LFS_OFFSET + QSPI_PARTITION_LFS_SIZE;
            const u32 test_end = test_addr + test_size;
             const u32 erase_count = (test_size + SPI_FLASH_SECTOR_SIZE - 1U) /
                                     SPI_FLASH_SECTOR_SIZE;
             const u32 erase_timeout_ms = 10000U;
             const u32 read_timeout_ms = flash_transfer_timeout_ms(SPI_FLASH_SECTOR_SIZE);
            uint8_t *write_data = pvPortMalloc(test_size);
           uint8_t *read_data = pvPortMalloc(test_size);

           if (write_data == NULL || read_data == NULL)
           {
               printf("memory allocation failed\n");
               if (write_data)
                   vPortFree(write_data);
               if (read_data)
                   vPortFree(read_data);
               return 1;
           }

            printf("benchmark erase region: [0x%08lx, 0x%08lx), size=%lu bytes\n",
                   (unsigned long)test_addr, (unsigned long)test_end,
                   (unsigned long)test_size);
            printf("benchmark write region: [0x%08lx, 0x%08lx), size=%lu bytes\n",
                   (unsigned long)test_addr, (unsigned long)test_end,
                   (unsigned long)test_size);
            printf("benchmark read region:  [0x%08lx, 0x%08lx), size=%lu bytes\n",
                   (unsigned long)test_addr, (unsigned long)test_end,
                   (unsigned long)test_size);

            // erase
            TickType_t start_time = xTaskGetTickCount();
            int erase_ret = 0;
            for (u32 i = 0; i < erase_count; i++)
            {
               erase_ret = flash_erase_block64K(flash,
                                                 test_addr + i * SPI_FLASH_SECTOR_SIZE);
               if (erase_ret != 0)
                   break;
            }
            TickType_t end_time = xTaskGetTickCount();
            printf("erase %lu bytes (%lu blocks) take %lu ms, timeout=%lu ms, ret=%d\n",
                   (unsigned long)test_size, (unsigned long)erase_count,
                   (unsigned long)((end_time - start_time) * portTICK_PERIOD_MS),
                   (unsigned long)(erase_count * erase_timeout_ms), erase_ret);

           //write
           for (u32 i = 0; i < test_size; i++)
           {
               write_data[i] = (uint8_t)(i & 0xFF);
           }
           start_time = xTaskGetTickCount();
            u32 written = flash_write(flash, test_addr, write_data, test_size);
            end_time = xTaskGetTickCount();
            printf("write region [0x%08lx, 0x%08lx), %lu/%lu bytes take %lu ms, timeout=%lu ms\n",
                   (unsigned long)test_addr, (unsigned long)test_end,
                   (unsigned long)written, (unsigned long)test_size,
                   (unsigned long)((end_time - start_time) * portTICK_PERIOD_MS),
                   (unsigned long)flash_transfer_timeout_ms(SPI_FLASH_PAGE_SIZE));

           //read
           start_time = xTaskGetTickCount();
            u32 read_size = flash_read(flash, test_addr, read_data, test_size);
            end_time = xTaskGetTickCount();
            printf("read region [0x%08lx, 0x%08lx), %lu/%lu bytes take %lu ms, timeout=%lu ms\n",
                   (unsigned long)test_addr, (unsigned long)test_end,
                   (unsigned long)read_size, (unsigned long)test_size,
                   (unsigned long)((end_time - start_time) * portTICK_PERIOD_MS),
                   (unsigned long)read_timeout_ms);
           //verify
            if (erase_ret == 0 && written == test_size && read_size == test_size &&
                memcmp(write_data, read_data, test_size) == 0)
           {
               printf("benchmark success: read data match written data\n");
           }
           else
           {
               printf("benchmark failed: read data does not match written data\n");
           }
           vPortFree(write_data);
           vPortFree(read_data);
       }
       else if (strcmp(qspi_flash_args.mode->sval[0], "read") == 0)
       {
           if ((qspi_flash_args.offset->count == 0) || (qspi_flash_args.size->count == 0))
           {
               printf("read offset or size is not set\n");
               return 1;
           }
           uint8_t *data = pvPortMalloc(qspi_flash_args.size->ival[0]);
           // sfud_read(flash, qspi_flash_args.offset->ival[0], qspi_flash_args.size->ival[0], data);
            flash_read_data(flash, qspi_flash_args.offset->ival[0], data, qspi_flash_args.size->ival[0], flash_transfer_timeout_ms(qspi_flash_args.size->ival[0]));
           print_flash_hex(qspi_flash_args.offset->ival[0], data, qspi_flash_args.size->ival[0]);
           vPortFree(data);
       }
       else if (strcmp(qspi_flash_args.mode->sval[0], "fastread") == 0)
       {
           if ((qspi_flash_args.offset->count == 0) || (qspi_flash_args.size->count == 0))
           {
               printf("read offset or size is not set\n");
               return 1;
           }
           uint8_t *data = pvPortMalloc(qspi_flash_args.size->ival[0]);
           if (data == NULL)
           {
               printf("memory allocation failed\n");
               return 1;
           }
            flash_fast_read_data(flash, qspi_flash_args.offset->ival[0], data, qspi_flash_args.size->ival[0], flash_transfer_timeout_ms(qspi_flash_args.size->ival[0]));
           print_flash_hex(qspi_flash_args.offset->ival[0], data, qspi_flash_args.size->ival[0]);
           vPortFree(data);
       }
       else if (strcmp(qspi_flash_args.mode->sval[0], "write") == 0)
       {
           if ((qspi_flash_args.offset->count == 0) || (qspi_flash_args.data->count == 0))
           {
               printf("write offset or data is not set\n");
               return 1;
           }
            flash_page_program(flash, qspi_flash_args.offset->ival[0], (u8 *)qspi_flash_args.data->sval[0], strlen(qspi_flash_args.data->sval[0]), flash_transfer_timeout_ms(strlen(qspi_flash_args.data->sval[0])));
       }
       else if (strcmp(qspi_flash_args.mode->sval[0], "erase") == 0)
       {
           flash_erase_block64K(flash, qspi_flash_args.offset->ival[0]);
       }
       else
       {
           printf("unsupported mode %s\n", qspi_flash_args.mode->sval[0]);
           return 1;
       }
   }
   return 0;
}



/**
* @brief 注册网络接口命令
*/
void register_sf_commands(void)
{
   qspi_flash_args.dev = arg_int0("d", "dev", "<dev_id>", "QSPI flash device ID (default: 0)");
   qspi_flash_args.offset = arg_int0("o", "offset", "<offset>", "QSPI flash device offset");
   qspi_flash_args.data = arg_str0("data", "data", "<data>", "QSPI flash device data");
   qspi_flash_args.size = arg_int0("s", "size", "<size>", "QSPI flash device size");
   qspi_flash_args.mode = arg_str0("m", "mode", "<mode>", "QSPI flash device mode run: erase read write fastread eraseall benchmark");
   qspi_flash_args.end = arg_end(4);
   const esp_console_cmd_t cmd = {
       .command = "sf",
       .help = "sf -d <dev_id> -o <offset> -s <size> -m read: Read QSPI flash device\n"
               "sf -d <dev_id> -o <offset> -s <size> -m fastread: Fast read QSPI flash device\n"
               "sf -d <dev_id> -o <offset> -data <data> -m write: Write QSPI flash device\n"
               "sf -d <dev_id> -o <offset> -s <size> -m erase: Erase QSPI flash sector\n"
               "sf -d <dev_id> -m eraseall: Erase all QSPI flash device\n"
               "sf -d <dev_id> -m benchmark: Benchmark QSPI flash device\n"
               "<dev_id> is optional, default: 0\n",
       .hint = NULL,
       .func = &sf_cmd_handler,
   };
   esp_console_cmd_register(&cmd);
}
