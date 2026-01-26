/* KallistiOS ##version##

   openat.c
   Copyright (C) 2026 Paul Cercueil

*/

#include <fcntl.h>
#include <kos/fs.h>

int openat(int fd, const char *path, int mode, ...) {
    if(fd == AT_FDCWD)
        fd = FILEHND_INVALID;

    return fs_openat(fd, path, mode);
}
