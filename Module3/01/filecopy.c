#define _POSIX_C_SOURCE 200809L

#include "filecopy.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int filecopy_open_source(const char *path, int *fd, off_t *file_size) {
    if (path == NULL || fd == NULL || file_size == NULL) {
        errno = EINVAL;
        return -1;
    }

    int source_fd = open(path, O_RDONLY);

    if (source_fd == -1) {
        return -1;
    }

    struct stat file_info;

    if (fstat(source_fd, &file_info) == -1) {
        int saved_errno = errno;
        close(source_fd);
        errno = saved_errno;
        return -1;
    }

    if (!S_ISREG(file_info.st_mode)) {
        close(source_fd);
        errno = EINVAL;
        return -1;
    }

    *fd = source_fd;
    *file_size = file_info.st_size;

    return 0;
}

int filecopy_build_copy_name(const char *source_path, char *copy_path, size_t capacity) {
    if (source_path == NULL || copy_path == NULL || capacity == 0) {
        errno = EINVAL;
        return -1;
    }

    int result = snprintf(
        copy_path,
        capacity,
        "%s.copy",
        source_path
    );

    if (result < 0 || (size_t)result >= capacity) {
        errno = ENAMETOOLONG;
        return -1;
    }

    return 0;
}

int filecopy_open_copy(const char *copy_path, int *fd) {
    if (copy_path == NULL || fd == NULL) {
        errno = EINVAL;
        return -1;
    }

    int copy_fd = open(
        copy_path,
        O_WRONLY | O_CREAT | O_TRUNC,
        0666
    );

    if (copy_fd == -1) {
        return -1;
    }

    *fd = copy_fd;

    return 0;
}

int filecopy_write_all(int fd, const void *buffer, size_t size) {
    const char *bytes = buffer;
    size_t total_written = 0;

    while (total_written < size) {
        ssize_t result = write(
            fd,
            bytes + total_written,
            size - total_written
        );

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (result == 0) {
            errno = EIO;
            return -1;
        }

        total_written += (size_t)result;
    }

    return 0;
}
