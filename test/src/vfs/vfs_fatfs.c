#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "FreeRTOS.h"

#include "vfs.h"

#define VFS_BACKEND_ROOT_PATH "/"

static int fatfs_open(const char *path, uint32_t flags, void **handle)
{
    FIL *file;
    BYTE open_flags;

    if (handle == NULL) return -1;
    *handle = NULL;
    file = (FIL *)pvPortMalloc(sizeof(*file));
    if (file == NULL) return -1;
    open_flags = (flags & VFS_OPEN_READ) ? FA_READ : (FA_WRITE | FA_CREATE_ALWAYS);
    if (f_open(file, path, open_flags) != FR_OK) {
        vPortFree(file);
        return -1;
    }
    *handle = file;
    return 0;
}

static int fatfs_read(void *handle, void *buffer, size_t size, size_t *read_size)
{
    UINT bytes_read = 0;
    if (handle == NULL || size > (size_t)((UINT)-1) ||
        f_read((FIL *)handle, buffer, (UINT)size, &bytes_read) != FR_OK) return -1;
    *read_size = (size_t)bytes_read;
    return 0;
}

static int fatfs_write(void *handle, const void *buffer, size_t size, size_t *write_size)
{
    UINT bytes_written = 0;
    if (handle == NULL || size > (size_t)((UINT)-1) ||
        f_write((FIL *)handle, buffer, (UINT)size, &bytes_written) != FR_OK) return -1;
    *write_size = (size_t)bytes_written;
    return 0;
}

static int fatfs_sync(void *handle)
{
    return (handle != NULL && f_sync((FIL *)handle) == FR_OK) ? 0 : -1;
}

static int fatfs_close(void *handle)
{
    int ret = -1;
    if (handle != NULL) {
        ret = (f_close((FIL *)handle) == FR_OK) ? 0 : -1;
        vPortFree(handle);
    }
    return ret;
}

static int fatfs_remove(const char *path)
{
    return f_unlink(path) == FR_OK ? 0 : -1;
}

static int fatfs_rename(const char *old_path, const char *new_path)
{
    FRESULT ret = f_rename(old_path, new_path);
    if (ret == FR_EXIST) {
        (void)f_unlink(new_path);
        ret = f_rename(old_path, new_path);
    }
    return ret == FR_OK ? 0 : -1;
}

static int fatfs_size(void *handle, size_t *file_size)
{
    if (handle == NULL || file_size == NULL) return -1;
    *file_size = (size_t)f_size((FIL *)handle);
    return 0;
}

static int fatfs_list_dir(const char *path)
{
    FRESULT res;
    DIR dir;
    FILINFO info;
    const char *target = path ? path : VFS_BACKEND_ROOT_PATH;

    printf("Listing directory (FATFS): %s\n", target);
    printf("%s\n", "----------------------------------------");
    printf("Type%20s%20s\n", "Size", "Name");
    printf("%s\n", "----------------------------------------");
    res = f_opendir(&dir, target);
    if (res != FR_OK) {
        printf("FATFS: Failed to open directory %s with error %d\n", target, res);
        return -1;
    }
    while (1) {
        res = f_readdir(&dir, &info);
        if (res != FR_OK || info.fname[0] == 0) break;
        if (strcmp(info.fname, ".") == 0 || strcmp(info.fname, "..") == 0) continue;
        if (info.fattrib & AM_DIR) printf("DIR%20s%20s\n", "-", info.fname);
        else printf("FILE%20lu%20s\n", info.fsize, info.fname);
    }
    f_closedir(&dir);
    if (res != FR_OK) {
        printf("FATFS: Failed to read directory %s with error %d\n", target, res);
        return -1;
    }
    printf("%s\n", "----------------------------------------");
    printf("Directory listing completed\n");
    return 0;
}

static int fatfs_mkdir(const char *path)
{
    FRESULT ret;

    if (path == NULL || path[0] == '\0') {
        printf("FATFS: Invalid directory path\n");
        return -1;
    }
    ret = f_mkdir(path);
    if (ret != FR_OK) {
        printf("FATFS: Failed to create directory %s with error %d\n", path, ret);
        return -1;
    }
    printf("FATFS: Directory %s created successfully\n", path);
    return 0;
}

static int fatfs_get_info(void)
{
    FATFS *pfs;
    DWORD free_clusters, free_sectors, total_sectors;
    FRESULT ret = f_getfree("", &free_clusters, &pfs);

    if (ret != FR_OK) {
        printf("FATFS: Failed to get file system info with error %d\n", ret);
        return -1;
    }
    total_sectors = (pfs->n_fatent - 2U) * pfs->csize;
    free_sectors = free_clusters * pfs->csize;
    printf("File System Information (FATFS):\n");
    printf("%s\n", "----------------------------------------");
    printf("Total space: %lu KB\n", (total_sectors * FF_MAX_SS) / 1024U);
    printf("Free space: %lu KB\n", (free_sectors * FF_MAX_SS) / 1024U);
    printf("Sector size: %u bytes\n", FF_MAX_SS);
    printf("Cluster size: %u sectors\n", pfs->csize);
    printf("%s\n", "----------------------------------------");
    return 0;
}

const vfs_backend_ops_t vfs_fatfs_backend = {
    .type = VFS_FS_FATFS, .name = "FATFS", .prefix = "fat:",
    .open = fatfs_open, .read = fatfs_read, .write = fatfs_write,
    .sync = fatfs_sync, .close = fatfs_close, .remove = fatfs_remove,
    .rename = fatfs_rename, .size = fatfs_size, .list_dir = fatfs_list_dir,
    .mkdir = fatfs_mkdir, .get_info = fatfs_get_info,
};
