
#ifndef FILECOPY_H
#define FILECOPY_H

#include <stddef.h>
#include <sys/types.h>

#define FILECOPY_PATH_MAX 4096

int filecopy_open_source(
    const char *path,
    int *fd,
    off_t *file_size
);

int filecopy_build_copy_name(
    const char *source_path,
    char *copy_path,
    size_t capacity
);

int filecopy_open_copy(
    const char *copy_path,
    int *fd
);

int filecopy_write_all(
    int fd,
    const void *buffer,
    size_t size
);

#endif
