#ifndef __VFS_H__
#define __VFS_H__

#include "stdint.h"
#include "stddef.h"

typedef enum {
    VFS_FS_FATFS = 0,
    VFS_FS_LFS = 1
} vfs_fs_type_t;

typedef struct vfs_file vfs_file_t;

typedef struct {
    vfs_fs_type_t type;
    const char *name;
    const char *prefix;
    int (*open)(const char *path, uint32_t flags, void **handle);
    int (*read)(void *handle, void *buffer, size_t size, size_t *read_size);
    int (*write)(void *handle, const void *buffer, size_t size, size_t *write_size);
    int (*sync)(void *handle);
    int (*close)(void *handle);
    int (*remove)(const char *path);
    int (*rename)(const char *old_path, const char *new_path);
    int (*size)(void *handle, size_t *file_size);
    int (*list_dir)(const char *path);
    int (*mkdir)(const char *path);
    int (*get_info)(void);
} vfs_backend_ops_t;

#define VFS_OPEN_READ       (1U << 0)
#define VFS_OPEN_WRITE      (1U << 1)
#define VFS_OPEN_ATOMIC     (1U << 2)
#define VFS_WAIT_FOREVER    UINT32_MAX

void vfs_init(void);
int vfs_register_backend(const vfs_backend_ops_t *backend);
void vfs_register_commands(void);
int vfs_open(const char *path, uint32_t flags, vfs_file_t **file);
int vfs_open_timeout(const char *path, uint32_t flags, uint32_t timeout_ms,
                    vfs_file_t **file);
int vfs_read(vfs_file_t *file, void *buffer, size_t size, size_t *read_size);
int vfs_write(vfs_file_t *file, const void *buffer, size_t size, size_t *write_size);
int vfs_sync(vfs_file_t *file);
int vfs_close(vfs_file_t *file);
int vfs_abort(vfs_file_t *file);
int vfs_remove(const char *path);
int vfs_rename(const char *old_path, const char *new_path);
void *vfs_read_content(const char *file_name, size_t *file_size);
int vfs_read_content_with_md5(const char *file_name, char **content,
                              size_t *file_size, uint8_t md5[16]);
int vfs_resolve_path(const char *path,
                     vfs_fs_type_t *fs_type,
                     const char **resolved_path);

#endif /* __VFS_H__ */
