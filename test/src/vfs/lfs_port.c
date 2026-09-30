#include "FreeRTOS.h"
#include "semphr.h"
#include "spiflash.h"
#include "lfs.h"


// variables used by the filesystem
lfs_t lfs;
lfs_file_t file;
static xSemaphoreHandle lfs_mutex;
static spiflash_handle_t *lfs_flash;


static int lfs_block_range_valid(const struct lfs_config *c,
                                 lfs_block_t block,
                                 lfs_off_t off,
                                 lfs_size_t size)
{
    return c != NULL && block < c->block_count && off <= c->block_size &&
           size <= c->block_size - off;
}

static int user_provided_block_device_read(const struct lfs_config *c,
                                           lfs_block_t block,
                                           lfs_off_t off,
                                           void *buffer,
                                           lfs_size_t size)
{
    if (!lfs_block_range_valid(c, block, off, size) || (size != 0U && buffer == NULL))
        return LFS_ERR_IO;

    uint32_t timeout_ms = size * 8 / FLASH_READ_WRITE_RATE + 1;
    if (lfs_flash != NULL && flash_fast_read_data(lfs_flash, QSPI_PARTITION_LFS_OFFSET + block * c->block_size + off, buffer, size,timeout_ms) == size)
        return LFS_ERR_OK;
    return LFS_ERR_IO;
}


static int user_provided_block_device_prog(const struct lfs_config *c,
                                           lfs_block_t block,
                                           lfs_off_t off,
                                           const void *buffer,
                                           lfs_size_t size)
{
    const uint8_t *data = buffer;
    uint32_t addr;

    if (!lfs_block_range_valid(c, block, off, size) || (size != 0U && buffer == NULL))
        return LFS_ERR_IO;

    addr = QSPI_PARTITION_LFS_OFFSET + block * c->block_size + off;
    while (size != 0U)
    {
        uint32_t page_remaining = SPI_FLASH_PAGE_SIZE - (addr & (SPI_FLASH_PAGE_SIZE - 1U));
        uint32_t write_size = size < page_remaining ? size : page_remaining;
        uint32_t timeout_ms = write_size * 8 / FLASH_READ_WRITE_RATE + 1;

        if (lfs_flash == NULL || flash_page_program(lfs_flash, addr, (uint8_t *)data, write_size, timeout_ms) != write_size)
            return LFS_ERR_IO;

        addr += write_size;
        data += write_size;
        size -= write_size;
    }

    return LFS_ERR_OK;
}

static int user_provided_block_device_erase(const struct lfs_config *c,
                                            lfs_block_t block)
{
    if (c == NULL || !lfs_block_range_valid(c, block, 0U, c->block_size))
        return LFS_ERR_IO;

    if (lfs_flash != NULL && flash_erase_block64K(lfs_flash, QSPI_PARTITION_LFS_OFFSET + block * c->block_size) == 0)
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
    .read_size = SPI_FLASH_PAGE_SIZE,
    .prog_size = SPI_FLASH_PAGE_SIZE,
    .block_size = SPI_FLASH_SECTOR_SIZE,
    .block_count = QSPI_PARTITION_LFS_SIZE / SPI_FLASH_SECTOR_SIZE,
    .cache_size = SPI_FLASH_PAGE_SIZE,
    .lookahead_size = 16,
    .block_cycles = 500,
};

// entry point
int lfs_init(void)
{
    // mount the filesystem
    spi_flash_init();
    lfs_flash = spiflash_get_handle(0);
    if (lfs_flash == NULL)
    {
        printf("lfs: flash handle unavailable\r\n");
        return LFS_ERR_IO;
    }
    lfs_mutex = xSemaphoreCreateMutex();
    configASSERT(lfs_mutex != NULL);
    int err = lfs_mount(&lfs, &cfg);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err)
    {
        printf("lfs: mount failed: %d, formatting partition\r\n", err);
        err = lfs_format(&lfs, &cfg);
        if (err)
        {
            printf("lfs: format failed: %d\r\n", err);
            return err;
        }

        err = lfs_mount(&lfs, &cfg);
        if (err)
        {
            printf("lfs: mount after format failed: %d\r\n", err);
            return err;
        }
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

    printf("lfs: mounted\r\n");
    return LFS_ERR_OK;
}
