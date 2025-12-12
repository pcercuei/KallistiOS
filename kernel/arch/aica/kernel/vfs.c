/* KallistiOS ##version##

   vfs.c
   Copyright (C) 2026 Paul Cercueil

   /sh4 virtual file-system
*/


#include <kos/fs.h>
#include <kos/rpc.h>
#include <aica/queue.h>

static void *fd_to_hnd(int fd) {
    return (void *)(fd + 1);
}

static int hnd_to_fd(void *ptr) {
    return (int)ptr - 1;
}

static void *vfs_sh4_open(vfs_handler_t *vfs, const char *fn, int mode) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_OPENFILE,
        .params = {
            [0] = (uint32_t)fn,
            [1] = mode,
        },
    };
    (void)vfs;

    return fd_to_hnd(rpc_execute(&aica_rpc, &cmd));
}

static int vfs_sh4_close(void *hnd) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_CLOSEFILE,
        .params = {
            [0] = hnd_to_fd(hnd),
        },
    };

    return rpc_execute(&aica_rpc, &cmd);
}

static int vfs_sh4_read(void *hnd, void *buffer, size_t cnt) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_READFILE,
        .params = {
            [0] = hnd_to_fd(hnd),
            [1] = (uint32_t)buffer,
            [2] = cnt,
        },
    };

    return rpc_execute(&aica_rpc, &cmd);
}

static ssize_t vfs_sh4_write(void *hnd, const void *buffer, size_t cnt) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_WRITEFILE,
        .params = {
            [0] = hnd_to_fd(hnd),
            [1] = (uint32_t)buffer,
            [2] = cnt,
        },
    };
    return rpc_execute(&aica_rpc, &cmd);
}

static off_t vfs_sh4_seek(void *hnd, off_t offset, int whence) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_SEEKFILE,
        .params = {
            [0] = hnd_to_fd(hnd),
            [1] = offset,
            [2] = whence,
        },
    };

    return rpc_execute(&aica_rpc, &cmd);
}

static off_t vfs_sh4_tell(void *hnd) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_TELLFILE,
        .params = {
            [0] = hnd_to_fd(hnd),
        },
    };

    return rpc_execute(&aica_rpc, &cmd);
}

static size_t vfs_sh4_total(void *hnd) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_TOTALFILE,
        .params = {
            [0] = hnd_to_fd(hnd),
        },
    };

    return rpc_execute(&aica_rpc, &cmd);
}

static _Thread_local dirent_t vfs_sh4_dirent;

static const dirent_t *vfs_sh4_readdir(void *hnd) {
    rpc_cmd_t cmd = {
        .cmd = AICA_CMD_READDIR,
        .params = {
            [0] = hnd_to_fd(hnd),
            [1] = (int)&vfs_sh4_dirent,
        },
    };

    return (const dirent_t *)rpc_execute(&aica_rpc, &cmd);
}

static vfs_handler_t vfs_sh4_vfs = {
    .nmmgr = {
        { "/sh4" },
        0,
        0x00010000,
        0,
        NMMGR_TYPE_VFS,
        NMMGR_LIST_INIT
    },
    .open = vfs_sh4_open,
    .close = vfs_sh4_close,
    .read = vfs_sh4_read,
    .write = vfs_sh4_write,
    .seek = vfs_sh4_seek,
    .tell = vfs_sh4_tell,
    .total = vfs_sh4_total,
    .readdir = vfs_sh4_readdir,
};

void vfs_sh4_init(void) {
    nmmgr_handler_add(&vfs_sh4_vfs.nmmgr);
}

void vfs_sh4_shutdown(void) {
    nmmgr_handler_remove(&vfs_sh4_vfs.nmmgr);
}
