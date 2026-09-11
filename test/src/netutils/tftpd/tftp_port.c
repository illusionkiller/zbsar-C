#include <stdio.h>

#include "tftp_server.h"
#include "vfs.h"

#define TFTP_LOCK_TIMEOUT_MS 100U

void *TFTP_Open(const char *fname, const char *mode, u8_t write)
{
    vfs_file_t *file = NULL;
    uint32_t flags = write ? (VFS_OPEN_WRITE | VFS_OPEN_ATOMIC) : VFS_OPEN_READ;

    (void)mode;
    if (vfs_open_timeout(fname, flags, TFTP_LOCK_TIMEOUT_MS, &file) != 0) {
        return NULL;
    }
    return file;
}

void TFTP_Close(void *handle)
{
    if (handle != NULL && vfs_close((vfs_file_t *)handle) != 0) {
        printf("tftp: Failed to commit or close file\n");
    }
}

void TFTP_Abort(void *handle)
{
    if (handle != NULL) {
        (void)vfs_abort((vfs_file_t *)handle);
    }
}

int TFTP_Read(void *handle, void *buf, int bytes)
{
    size_t read_size = 0;

    if (handle == NULL || buf == NULL || bytes < 0 ||
        vfs_read((vfs_file_t *)handle, buf, (size_t)bytes, &read_size) != 0) {
        return -1;
    }
    return (int)read_size;
}

int TFTP_Write(void *handle, struct pbuf *p)
{
    size_t write_size = 0;

    if (handle == NULL || p == NULL || p->len != p->tot_len ||
        vfs_write((vfs_file_t *)handle, p->payload, p->tot_len, &write_size) != 0 ||
        write_size != p->tot_len) {
        return -1;
    }
    return (int)write_size;
}

void tftpd_init(void)
{
    static struct tftp_context tftpd = {0};

    tftpd.open = TFTP_Open;
    tftpd.close = TFTP_Close;
    tftpd.abort = TFTP_Abort;
    tftpd.read = TFTP_Read;
    tftpd.write = TFTP_Write;
    tftp_init(&tftpd);
}
