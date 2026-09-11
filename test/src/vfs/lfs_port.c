#include "FreeRTOS.h"
#include "semphr.h"
#include "spiflash.h"
#include "lfs.h"


// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;
static xSemaphoreHandle lfs_mutex;
#define FLASH_READ_WRITE_RATE 0x1000 // 1Mbpms
static int user_provided_block_device_read(const struct lfs_config *c,
                                           lfs_block_t block,
                                           lfs_off_t off,
                                           void *buffer,
                                           lfs_size_t size)
{
    uint32_t timeout_ms = size * 8 / FLASH_READ_WRITE_RATE + 1;
    if (flash_fast_read_data(0,QSPI_PARTITION_LFS_OFFSET + block * c->block_size + off, buffer, size,timeout_ms) == size)
        return LFS_ERR_OK;
    return LFS_ERR_IO;
}


static int user_provided_block_device_prog(const struct lfs_config *c,
                                           lfs_block_t block,
                                           lfs_off_t off,
                                           const void *buffer,
                                           lfs_size_t size)
{
	uint32_t timeout_ms = size * 8 / FLASH_READ_WRITE_RATE + 1;
    if (flash_page_program(0,QSPI_PARTITION_LFS_OFFSET + block * c->block_size + off, (uint8_t *)buffer, size, timeout_ms) == size)
        return LFS_ERR_OK;
    return LFS_ERR_IO;
}

static int user_provided_block_device_erase(const struct lfs_config *c,
                                            lfs_block_t block)
{
    if (flash_erase_block64K(0,QSPI_PARTITION_LFS_OFFSET + block * c->block_size) == 0)
        return LFS_ERR_OK;
    return LFS_ERR_IO;
}

static int user_provided_block_device_sync(const struct lfs_config *c)
{
    return LFS_ERR_OK;
}

static int user_provided_lock(const struct lfs_config *c)
{
    xSemaphoreTake(lfs_mutex, portMAX_DELAY);
    return LFS_ERR_OK;
}

static int user_provided_unlock(const struct lfs_config *c)
{
    // implement your block device unlock function here
    // unlock the block device after thread-safe access
    // return 0 on success, or a negative error code on failure
    xSemaphoreGive(lfs_mutex);
    return LFS_ERR_OK;
}

// configuration of the filesystem is provided by this struct
const struct lfs_config cfg = {
    // block device operations
    .read = user_provided_block_device_read,
    .prog = user_provided_block_device_prog,
    .erase = user_provided_block_device_erase,
    .sync = user_provided_block_device_sync,
#ifdef LFS_THREADSAFE
    .lock = user_provided_lock,
    .unlock = user_provided_unlock,
#endif

    // block device configuration
    .read_size = 256,
    .prog_size = 256,
    .block_size = 1024*64,
    .block_count = QSPI_PARTITION_LFS_SIZE / (1024*64), // 6MB for LFS partition
    .cache_size = 1024*64,
    .lookahead_size = 16,
    .block_cycles = 500,
};

// entry point
int lfs_init(void)
{
    // mount the filesystem
    spi_flash_init();
    lfs_mutex = xSemaphoreCreateMutex();
    configASSERT(lfs_mutex != NULL);
    int err = lfs_mount(&lfs, &cfg);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err)
    {
        lfs_format(&lfs, &cfg);
        lfs_mount(&lfs, &cfg);
    }
//
//    // read current count
//    uint32_t boot_count = 0;
//    lfs_file_open(&lfs, &file, "boot_count", LFS_O_RDWR | LFS_O_CREAT);
//    lfs_file_read(&lfs, &file, &boot_count, sizeof(boot_count));
//
//    // update boot count
//    boot_count += 1;
//    lfs_file_rewind(&lfs, &file);
//    lfs_file_write(&lfs, &file, &boot_count, sizeof(boot_count));
//
//    // remember the storage is not updated until the file is closed successfully
//    lfs_file_close(&lfs, &file);

    // release any resources we were using
//    lfs_unmount(&lfs);

    return 0;
}
