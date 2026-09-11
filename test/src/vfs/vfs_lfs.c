#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"

#include "vfs.h"
#include "lfs.h"

#define VFS_BACKEND_ROOT_PATH "/"

extern lfs_t lfs;

static int lfs_open(const char *path, uint32_t flags, void **handle)
{
    lfs_file_t *file;
    int open_flags;
    if (handle == NULL) return -1;
    *handle = NULL;
    file = (lfs_file_t *)pvPortMalloc(sizeof(*file));
    if (file == NULL) return -1;
    open_flags = (flags & VFS_OPEN_READ) ? LFS_O_RDONLY :
                 (LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
    if (lfs_file_open(&lfs, file, path, open_flags) < 0) {
        vPortFree(file);
        return -1;
    }
    *handle = file;
    return 0;
}

static int lfs_read(void *handle, void *buffer, size_t size, size_t *read_size)
{
    lfs_ssize_t ret;
    if (handle == NULL) return -1;
    ret = lfs_file_read(&lfs, (lfs_file_t *)handle, buffer, size);
    if (ret < 0) return -1;
    *read_size = (size_t)ret;
    return 0;
}

static int lfs_write(void *handle, const void *buffer, size_t size, size_t *write_size)
{
    lfs_ssize_t ret;
    if (handle == NULL) return -1;
    ret = lfs_file_write(&lfs, (lfs_file_t *)handle, buffer, size);
    if (ret < 0) return -1;
    *write_size = (size_t)ret;
    return 0;
}

static int lfs_sync(void *handle)
{
    return (handle != NULL && lfs_file_sync(&lfs, (lfs_file_t *)handle) >= 0) ? 0 : -1;
}

static int lfs_close(void *handle)
{
    int ret = -1;
    if (handle != NULL) {
        ret = (lfs_file_close(&lfs, (lfs_file_t *)handle) >= 0) ? 0 : -1;
        vPortFree(handle);
    }
    return ret;
}

static int lfs_backend_remove(const char *path)
{
    return lfs_remove(&lfs, path) >= 0 ? 0 : -1;
}

static int lfs_backend_rename(const char *old_path, const char *new_path)
{
    return lfs_rename(&lfs, old_path, new_path) >= 0 ? 0 : -1;
}

static int lfs_size(void *handle, size_t *file_size)
{
    lfs_soff_t size;
    if (handle == NULL || file_size == NULL) return -1;
    size = lfs_file_size(&lfs, (lfs_file_t *)handle);
    if (size < 0) return -1;
    *file_size = (size_t)size;
    return 0;
}

static int lfs_list_dir(const char *path)
{
    lfs_dir_t dir;
    struct lfs_info info;
    const char *target = path ? path : VFS_BACKEND_ROOT_PATH;
    int ret;

    printf("Listing directory (LFS): %s\n", target);
    printf("%s\n", "----------------------------------------");
    printf("Type%20s%20s\n", "Size", "Name");
    printf("%s\n", "----------------------------------------");
    ret = lfs_dir_open(&lfs, &dir, target);
    if (ret < 0) {
        printf("LFS: Failed to open directory %s with error %d\n", target, ret);
        return -1;
    }
    while ((ret = lfs_dir_read(&lfs, &dir, &info)) > 0) {
        if (strcmp(info.name, ".") == 0 || strcmp(info.name, "..") == 0) continue;
        if (info.type == LFS_TYPE_DIR) printf("DIR%20s%20s\n", "-", info.name);
        else printf("FILE%20lu%20s\n", (unsigned long)info.size, info.name);
    }
    lfs_dir_close(&lfs, &dir);
    if (ret < 0) {
        printf("LFS: Failed to read directory %s with error %d\n", target, ret);
        return -1;
    }
    printf("%s\n", "----------------------------------------");
    printf("Directory listing completed\n");
    return 0;
}

static int lfs_backend_mkdir(const char *path)
{
    int ret;

    if (path == NULL || path[0] == '\0') {
        printf("LFS: Invalid directory path\n");
        return -1;
    }
    ret = lfs_mkdir(&lfs, path);
    if (ret < 0) {
        printf("LFS: Failed to create directory %s with error %d\n", path, ret);
        return -1;
    }
    printf("LFS: Directory %s created successfully\n", path);
    return 0;
}

static int lfs_get_info(void)
{
    struct lfs_fsinfo fsinfo;
    lfs_ssize_t used_blocks;
    uint64_t total_bytes;
    uint64_t used_bytes;
    uint64_t free_bytes;
    int ret = lfs_fs_stat(&lfs, &fsinfo);

    if (ret < 0) {
        printf("LFS: Failed to get file system info with error %d\n", ret);
        return -1;
    }
    used_blocks = lfs_fs_size(&lfs);
    if (used_blocks < 0) {
        printf("LFS: Failed to get file system used size with error %ld\n", (long)used_blocks);
        return -1;
    }
    total_bytes = (uint64_t)fsinfo.block_size * fsinfo.block_count;
    used_bytes = (uint64_t)used_blocks * fsinfo.block_size;
    free_bytes = (total_bytes >= used_bytes) ? total_bytes - used_bytes : 0U;
    printf("File System Information (LFS):\n");
    printf("%s\n", "----------------------------------------");
    printf("Total space: %lu KB\n", (unsigned long)(total_bytes / 1024U));
    printf("Free space: %lu KB\n", (unsigned long)(free_bytes / 1024U));
    printf("Block size: %lu bytes\n", (unsigned long)fsinfo.block_size);
    printf("Block count: %lu\n", (unsigned long)fsinfo.block_count);
    printf("%s\n", "----------------------------------------");
    return 0;
}

const vfs_backend_ops_t vfs_lfs_backend = {
    .type = VFS_FS_LFS, .name = "LFS", .prefix = "lfs:",
    .open = lfs_open, .read = lfs_read, .write = lfs_write,
    .sync = lfs_sync, .close = lfs_close, .remove = lfs_backend_remove,
    .rename = lfs_backend_rename, .size = lfs_size, .list_dir = lfs_list_dir,
    .mkdir = lfs_backend_mkdir, .get_info = lfs_get_info,
};
