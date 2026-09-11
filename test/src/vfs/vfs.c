#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "shell.h"
#include "vfs.h"
#include "vfs_backends.h"
#include "tiny_md5.h"
#include "log.h"
#define VFS_ROOT_PATH "/"
#define VFS_MD5_BUFFER_SIZE 256U
#define VFS_PATH_MAX 256U
#define VFS_MAX_BACKENDS 4U

static SemaphoreHandle_t vfs_mutex;
static const vfs_backend_ops_t *vfs_backends[VFS_MAX_BACKENDS];
static size_t vfs_backend_count;

struct vfs_file {
    const vfs_backend_ops_t *backend;
    uint32_t flags;
    void *backend_handle;
    char final_path[VFS_PATH_MAX];
    char temp_path[VFS_PATH_MAX];
};

static const vfs_backend_ops_t *vfs_find_backend(vfs_fs_type_t type)
{
    size_t i;

    for (i = 0; i < vfs_backend_count; ++i) {
        if (vfs_backends[i]->type == type) {
            return vfs_backends[i];
        }
    }
    return NULL;
}

static const vfs_backend_ops_t *vfs_find_backend_by_name(const char *name)
{
    size_t i;

    if (name == NULL) {
        return NULL;
    }
    for (i = 0; i < vfs_backend_count; ++i) {
        const vfs_backend_ops_t *backend = vfs_backends[i];
        size_t prefix_len = strlen(backend->prefix);

        if (strcmp(name, backend->name) == 0 ||
            (prefix_len > 1U && strlen(name) == prefix_len - 1U &&
             strncmp(name, backend->prefix, prefix_len - 1U) == 0)) {
            return backend;
        }
    }
    return NULL;
}

int vfs_register_backend(const vfs_backend_ops_t *backend)
{
    if (backend == NULL || backend->name == NULL || backend->prefix == NULL ||
        backend->open == NULL || backend->read == NULL || backend->write == NULL ||
        backend->sync == NULL || backend->close == NULL || backend->remove == NULL ||
        backend->rename == NULL || backend->size == NULL ||
        vfs_find_backend(backend->type) != NULL ||
        vfs_backend_count >= VFS_MAX_BACKENDS) {
        return -1;
    }

    vfs_backends[vfs_backend_count++] = backend;
    return 0;
}

void vfs_init(void)
{
    if (vfs_mutex == NULL) {
        vfs_mutex = xSemaphoreCreateRecursiveMutex();
        if (vfs_mutex == NULL) {
            log_error("Failed to create VFS mutex");
        }
    }

    if (vfs_find_backend(VFS_FS_FATFS) == NULL &&
        vfs_register_backend(&vfs_fatfs_backend) != 0) {
        log_error("Failed to register FATFS backend");
    }
    if (vfs_find_backend(VFS_FS_LFS) == NULL &&
        vfs_register_backend(&vfs_lfs_backend) != 0) {
        log_error("Failed to register LFS backend");
    }
}

static int vfs_lock_take(uint32_t timeout_ms)
{
    TickType_t timeout_ticks;

    if (vfs_mutex == NULL) {
        return -1;
    }

    if (timeout_ms == VFS_WAIT_FOREVER) {
        timeout_ticks = portMAX_DELAY;
    } else {
        timeout_ticks = pdMS_TO_TICKS(timeout_ms);
        if (timeout_ms > 0U && timeout_ticks == 0) {
            timeout_ticks = 1;
        }
    }

    return (xSemaphoreTakeRecursive(vfs_mutex, timeout_ticks) == pdTRUE) ? 0 : -1;
}

static void vfs_lock_give(void)
{
    if (vfs_mutex != NULL) {
        (void)xSemaphoreGiveRecursive(vfs_mutex);
    }
}

static const char *vfs_fs_name(vfs_fs_type_t fs_type)
{
    const vfs_backend_ops_t *backend = vfs_find_backend(fs_type);

    return (backend != NULL) ? backend->name : "unknown";
}

int vfs_resolve_path(const char *path,
                              vfs_fs_type_t *fs_type,
                              const char **resolved_path)
{
    const vfs_backend_ops_t *default_backend;
    size_t i;
    static const char *root_path = VFS_ROOT_PATH;

    if (!fs_type || !resolved_path) {
        return -1;
    }

    default_backend = vfs_find_backend(VFS_FS_FATFS);
    if (default_backend == NULL) {
        return -1;
    }

    if (!path || path[0] == '\0') {
        *fs_type = default_backend->type;
        *resolved_path = root_path;
        return 0;
    }

    for (i = 0; i < vfs_backend_count; ++i) {
        const vfs_backend_ops_t *backend = vfs_backends[i];
        size_t prefix_len = strlen(backend->prefix);

        if (strncmp(path, backend->prefix, prefix_len) == 0) {
            *fs_type = backend->type;
            *resolved_path = (path[prefix_len] != '\0') ?
                &path[prefix_len] : root_path;
            return 0;
        }
    }

    *fs_type = default_backend->type;
    *resolved_path = path;
    return 0;
}

static int vfs_build_path(char *dest, size_t dest_size, const char *path,
                          uint32_t flags)
{
    int written;

    written = snprintf(dest, dest_size, "%s%s", path,
                       (flags & VFS_OPEN_ATOMIC) ? ".uploading" : "");
    return (written >= 0 && (size_t)written < dest_size) ? 0 : -1;
}

int vfs_open_timeout(const char *path, uint32_t flags, uint32_t timeout_ms,
                    vfs_file_t **file)
{
    vfs_fs_type_t fs_type;
    const char *resolved_path;
    const vfs_backend_ops_t *backend;
    vfs_file_t *handle;

    if (file == NULL || path == NULL ||
        ((flags & (VFS_OPEN_READ | VFS_OPEN_WRITE)) == 0U) ||
        ((flags & VFS_OPEN_READ) && (flags & VFS_OPEN_WRITE)) ||
        ((flags & VFS_OPEN_ATOMIC) && !(flags & VFS_OPEN_WRITE))) {
        return -1;
    }
    *file = NULL;

    if (vfs_resolve_path(path, &fs_type, &resolved_path) != 0 ||
        strlen(resolved_path) >= VFS_PATH_MAX) {
        return -1;
    }
    backend = vfs_find_backend(fs_type);
    if (backend == NULL) {
        return -1;
    }
    if (vfs_lock_take(timeout_ms) != 0) {
        return -1;
    }

    handle = (vfs_file_t *)pvPortMalloc(sizeof(*handle));
    if (handle == NULL) {
        vfs_lock_give();
        return -1;
    }
    memset(handle, 0, sizeof(*handle));
    handle->backend = backend;
    handle->flags = flags;
    (void)snprintf(handle->final_path, sizeof(handle->final_path), "%s", resolved_path);
    if (vfs_build_path(handle->temp_path, sizeof(handle->temp_path), resolved_path, flags) != 0) {
        vPortFree(handle);
        vfs_lock_give();
        return -1;
    }

    if (backend->open(handle->temp_path, flags, &handle->backend_handle) != 0) {
        vPortFree(handle);
        vfs_lock_give();
        return -1;
    }

    *file = handle;
    return 0;
}

int vfs_open(const char *path, uint32_t flags, vfs_file_t **file)
{
    return vfs_open_timeout(path, flags, VFS_WAIT_FOREVER, file);
}

int vfs_read(vfs_file_t *file, void *buffer, size_t size, size_t *read_size)
{
    if (file == NULL || buffer == NULL || read_size == NULL ||
        !(file->flags & VFS_OPEN_READ)) {
        return -1;
    }

    return file->backend->read(file->backend_handle, buffer, size, read_size);
}

int vfs_write(vfs_file_t *file, const void *buffer, size_t size, size_t *write_size)
{
    if (file == NULL || buffer == NULL || write_size == NULL ||
        !(file->flags & VFS_OPEN_WRITE)) {
        return -1;
    }

    return file->backend->write(file->backend_handle, buffer, size, write_size);
}

int vfs_sync(vfs_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    return file->backend->sync(file->backend_handle);
}

int vfs_close(vfs_file_t *file)
{
    int ret = 0;

    if (file == NULL) {
        return -1;
    }
    if (file->flags & VFS_OPEN_WRITE) {
        if (vfs_sync(file) != 0) {
            ret = -1;
        }
    }

    if (file->backend->close(file->backend_handle) != 0) {
        ret = -1;
    }
    if (ret == 0 && (file->flags & VFS_OPEN_ATOMIC) &&
        file->backend->rename(file->temp_path, file->final_path) != 0) {
        ret = -1;
    }

    vPortFree(file);
    vfs_lock_give();
    return ret;
}

int vfs_abort(vfs_file_t *file)
{
    if (file == NULL) {
        return -1;
    }
    (void)file->backend->close(file->backend_handle);
    if (file->flags & VFS_OPEN_ATOMIC) {
        (void)file->backend->remove(file->temp_path);
    }
    vPortFree(file);
    vfs_lock_give();
    return 0;
}

int vfs_remove(const char *path)
{
    vfs_fs_type_t fs_type;
    const char *resolved_path;
    const vfs_backend_ops_t *backend;
    int ret = -1;

    if (vfs_resolve_path(path, &fs_type, &resolved_path) != 0 ||
        vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }
    backend = vfs_find_backend(fs_type);
    if (backend != NULL) {
        ret = backend->remove(resolved_path);
    }
    vfs_lock_give();
    return ret;
}

int vfs_rename(const char *old_path, const char *new_path)
{
    vfs_fs_type_t old_type, new_type;
    const char *old_resolved, *new_resolved;
    const vfs_backend_ops_t *backend;
    int ret = -1;

    if (vfs_resolve_path(old_path, &old_type, &old_resolved) != 0 ||
        vfs_resolve_path(new_path, &new_type, &new_resolved) != 0 ||
        old_type != new_type || vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }
    backend = vfs_find_backend(old_type);
    if (backend != NULL) {
        ret = backend->rename(old_resolved, new_resolved);
    }
    vfs_lock_give();
    return ret;
}

static int vfs_list_dir(const char *path)
{
    vfs_fs_type_t fs_type;
    const char *resolved_path;
    const vfs_backend_ops_t *backend;
    int ret;

    if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }

    if (vfs_resolve_path(path, &fs_type, &resolved_path) != 0) {
        printf("Invalid path\n");
        vfs_lock_give();
        return -1;
    }

    backend = vfs_find_backend(fs_type);
    ret = (backend != NULL && backend->list_dir != NULL) ?
        backend->list_dir(resolved_path) : -1;
    vfs_lock_give();
    return ret;
}

static int vfs_delete_path(const char *path)
{
    vfs_fs_type_t fs_type;
    const char *resolved_path;
    const vfs_backend_ops_t *backend;
    int ret;

    if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }

    if (vfs_resolve_path(path, &fs_type, &resolved_path) != 0) {
        printf("Invalid path\n");
        vfs_lock_give();
        return -1;
    }

    backend = vfs_find_backend(fs_type);
    ret = (backend != NULL) ? backend->remove(resolved_path) : -1;
    vfs_lock_give();
    return ret;
}

static int vfs_mkdir_dispatch(const char *path)
{
    vfs_fs_type_t fs_type;
    const char *resolved_path;
    const vfs_backend_ops_t *backend;
    int ret;

    if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }

    if (vfs_resolve_path(path, &fs_type, &resolved_path) != 0) {
        printf("Invalid path\n");
        vfs_lock_give();
        return -1;
    }

    backend = vfs_find_backend(fs_type);
    ret = (backend != NULL && backend->mkdir != NULL) ?
        backend->mkdir(resolved_path) : -1;
    vfs_lock_give();
    return ret;
}

static int vfs_rename_dispatch(const char *old_path, const char *new_path)
{
    vfs_fs_type_t old_fs_type;
    vfs_fs_type_t new_fs_type;
    const char *resolved_old_path;
    const char *resolved_new_path;
    const vfs_backend_ops_t *backend;
    int ret;

    if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }

    if (vfs_resolve_path(old_path, &old_fs_type, &resolved_old_path) != 0 ||
        vfs_resolve_path(new_path, &new_fs_type, &resolved_new_path) != 0) {
        printf("Invalid path\n");
        vfs_lock_give();
        return -1;
    }

    if (old_fs_type != new_fs_type) {
        printf("Cross-filesystem rename is not supported (%s -> %s)\n",
               vfs_fs_name(old_fs_type),
               vfs_fs_name(new_fs_type));
        vfs_lock_give();
        return -1;
    }

    backend = vfs_find_backend(old_fs_type);
    ret = (backend != NULL) ?
        backend->rename(resolved_old_path, resolved_new_path) : -1;
    vfs_lock_give();
    return ret;
}

static int vfs_get_info_dispatch(const char *name)
{
    const vfs_backend_ops_t *backend;
    int ret;

    if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }

    backend = vfs_find_backend_by_name(name);
    ret = (backend != NULL && backend->get_info != NULL) ?
        backend->get_info() : -1;
    if (ret != 0 && backend == NULL) {
        printf("Unsupported filesystem: %s\n", name);
    }
    vfs_lock_give();
    return ret;
}

static int ls_cmd(int argc, char **argv)
{
    const char *path = NULL;

    if (argc >= 2) {
        path = argv[1];
    }

    return vfs_list_dir(path);
}

static int rm_cmd(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: rm <file_path>\n");
        return 1;
    }

    return vfs_delete_path(argv[1]);
}

static int mkdir_cmd(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: mkdir <dir_path>\n");
        return 1;
    }

    return vfs_mkdir_dispatch(argv[1]);
}

static int mv_cmd(int argc, char **argv)
{
    if (argc != 3) {
        printf("Usage: mv <old_path> <new_path>\n");
        return 1;
    }

    return vfs_rename_dispatch(argv[1], argv[2]);
}

static int df_cmd(int argc, char **argv)
{
    if (argc == 1) {
        size_t i;
        int ret = 0;

        if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
            return -1;
        }
        for (i = 0; i < vfs_backend_count; ++i) {
            if (vfs_backends[i]->get_info == NULL ||
                vfs_backends[i]->get_info() != 0) {
                ret = -1;
            }
        }
        vfs_lock_give();
        return ret;
    }

    if (argc == 2) {
        return vfs_get_info_dispatch(argv[1]);
    }

    printf("Usage: df [fat|lfs]\n");
    return 1;
}

static void register_ls_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "ls",
        .help = "List directory contents",
        .hint = "[fat:/path | lfs:/path]",
        .func = &ls_cmd,
    };
    esp_console_cmd_register(&cmd);
}

static void register_rm_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "rm",
        .help = "Delete a file",
        .hint = "<fat:/file_path | lfs:/file_path>",
        .func = &rm_cmd,
    };
    esp_console_cmd_register(&cmd);
}

static void register_mkdir_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "mkdir",
        .help = "Create a directory",
        .hint = "<fat:/dir_path | lfs:/dir_path>",
        .func = &mkdir_cmd,
    };
    esp_console_cmd_register(&cmd);
}

static void register_mv_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "mv",
        .help = "Rename file or directory",
        .hint = "<old_path> <new_path>",
        .func = &mv_cmd,
    };
    esp_console_cmd_register(&cmd);
}

static void register_df_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "df",
        .help = "Display file system information",
        .hint = "[fat|lfs]",
        .func = &df_cmd,
    };
    esp_console_cmd_register(&cmd);
}

void *vfs_read_content(const char *file_name, size_t *file_size)
{
    vfs_fs_type_t fs_type;
    const char *resolved_path;
    const vfs_backend_ops_t *backend;
    void *handle = NULL;
    char *content = NULL;
    size_t bytes_read = 0;

    if (!file_name || !file_size) {
        return NULL;
    }
    *file_size = 0;

    if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return NULL;
    }

    if (vfs_resolve_path(file_name, &fs_type, &resolved_path) != 0) {
        vfs_lock_give();
        return NULL;
    }

    backend = vfs_find_backend(fs_type);
    if (backend == NULL || backend->open(resolved_path, VFS_OPEN_READ, &handle) != 0 ||
        backend->size(handle, file_size) != 0) {
        log_error("Failed to open file %s on %s", resolved_path,
                  (backend != NULL) ? backend->name : "unknown");
        if (handle != NULL) {
            (void)backend->close(handle);
        }
        vfs_lock_give();
        return NULL;
    }

    content = (char *)pvPortMalloc(*file_size + 1U);
    if (content == NULL ||
        backend->read(handle, content, *file_size, &bytes_read) != 0 ||
        bytes_read != *file_size) {
        log_error("Failed to read file content %s", resolved_path);
        vPortFree(content);
        (void)backend->close(handle);
        *file_size = 0;
        vfs_lock_give();
        return NULL;
    }

    content[*file_size] = '\0';
    (void)backend->close(handle);
    vfs_lock_give();
    return content;
}

int vfs_read_content_with_md5(const char *file_name, char **content,
                              size_t *file_size, uint8_t md5[16])
{
    tiny_md5_context ctx;
    char *buffer;

    if (content == NULL || file_size == NULL || md5 == NULL) {
        return -1;
    }
    *content = NULL;
    *file_size = 0;

    /* Keep the VFS lock across both the file snapshot and checksum. */
    if (vfs_lock_take(VFS_WAIT_FOREVER) != 0) {
        return -1;
    }

    buffer = (char *)vfs_read_content(file_name, file_size);
    if (buffer == NULL) {
        vfs_lock_give();
        return -1;
    }

    tiny_md5_starts(&ctx);
    tiny_md5_update(&ctx, (uint8_t *)buffer, (int)*file_size);
    tiny_md5_finish(&ctx, md5);
    *content = buffer;
    vfs_lock_give();
    return 0;
}

static int is_text_content(const char *content, size_t size)
{
    size_t i;

    for (i = 0; i < size; ++i) {
        unsigned char ch = (unsigned char)content[i];

        if (ch == '\0') {
            return 0;
        }

        if (ch < 0x20 && ch != '\r' && ch != '\n' && ch != '\t') {
            return 0;
        }
    }

    return 1;
}

static int cat_cmd(int argc, char **argv)
{
    size_t file_size;
    char *content;

    if (argc != 2) {
        printf("Usage: cat <file_path>\n");
        return 1;
    }

    content = (char *)vfs_read_content(argv[1], &file_size);
    if (content == NULL) {
        printf("Failed to read file %s\n", argv[1]);
        return -1;
    }

    if (!is_text_content(content, file_size)) {
        printf("cat: %s is not a supported text file\n", argv[1]);
        vPortFree(content);
        return -1;
    }

    content[file_size] = '\0';
    printf("%s\r\n", content);
    vPortFree(content);
    return 0;
}

static void register_cat_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "cat",
        .help = "Display file content",
        .hint = "<fat:/file_path | lfs:/file_path>",
        .func = &cat_cmd,
    };
    esp_console_cmd_register(&cmd);
}

static int md5_cmd(int argc, char **argv)
{
    uint8_t md5[16];
    char *content = NULL;
    size_t content_size = 0;
    int i;
    int ret;

    if (argc != 2) {
        printf("Usage: md5 <file_path>\n");
        return 1;
    }

    ret = vfs_read_content_with_md5(argv[1], &content, &content_size, md5);
    if (ret != 0) {
        printf("Failed to calculate MD5 for file %s\n", argv[1]);
        return -1;
    }
    vPortFree(content);

    printf("MD5 of %s: ", argv[1]);
    for (i = 0; i < 16; i++) {
        printf("%02x", md5[i]);
    }
    printf("\n");

    return 0;
}

static void register_md5_cmd(void)
{
    const esp_console_cmd_t cmd = {
        .command = "md5",
        .help = "Calculate MD5 checksum of a file",
        .hint = "<fat:/file_path | lfs:/file_path>",
        .func = &md5_cmd,
    };
    esp_console_cmd_register(&cmd);
}

void register_vfs_commands(void)
{
    register_ls_cmd();
    register_rm_cmd();
    register_mkdir_cmd();
    register_mv_cmd();
    register_df_cmd();
    register_cat_cmd();
    register_md5_cmd();
}
