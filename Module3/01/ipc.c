#define _POSIX_C_SOURCE 200809L

#include "ipc.h"
#include "filecopy.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define TRANSFER_BLOCK_SIZE 4096

typedef enum {
    MSG_READY = 1,
    MSG_FILE,
    MSG_ACCEPT,
    MSG_SKIP,
    MSG_DONE,
    MSG_ERROR,
    MSG_END
} MessageType;

typedef struct {
    int type;
    off_t file_size;
    int error_code;
    char filename[FILECOPY_PATH_MAX];
} IpcMessage;

static int write_all(int fd, const void *buffer, size_t size) {
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

static int read_all(int fd, void *buffer, size_t size) {
    char *bytes = buffer;
    size_t total_read = 0;

    while (total_read < size) {
        ssize_t result = read(
            fd,
            bytes + total_read,
            size - total_read
        );

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (result == 0) {
            errno = EPIPE;
            return -1;
        }

        total_read += (size_t)result;
    }

    return 0;
}

static int send_message(int fd, const IpcMessage *message) {
    return write_all(fd, message, sizeof(*message));
}

static int receive_message(int fd, IpcMessage *message) {
    return read_all(fd, message, sizeof(*message));
}

static int child_process(int read_fd, int write_fd) {
    char buffer[TRANSFER_BLOCK_SIZE];

    for (;;) {
        IpcMessage message = {0};
        message.type = MSG_READY;

        if (send_message(write_fd, &message) == -1) {
            perror("Child: send READY");
            return 1;
        }

        if (receive_message(read_fd, &message) == -1) {
            perror("Child: receive message");
            return 1;
        }

        if (message.type == MSG_END) {
            return 0;
        }

        if (message.type == MSG_SKIP) {
            continue;
        }

        if (message.type != MSG_FILE ||
            message.file_size < 0 ||
            memchr(message.filename, '\0',
                   sizeof(message.filename)) == NULL) {
            fprintf(stderr, "Child: invalid file information\n");
            return 1;
        }

        char copy_path[FILECOPY_PATH_MAX + sizeof(".copy")];
        int copy_fd;

        if (filecopy_build_copy_name(
                message.filename,
                copy_path,
                sizeof(copy_path)) == -1 ||
            filecopy_open_copy(copy_path, &copy_fd) == -1) {

            int saved_errno = errno;

            IpcMessage response = {0};
            response.type = MSG_ERROR;
            response.error_code = saved_errno;

            if (send_message(write_fd, &response) == -1) {
                perror("Child: send error");
                return 1;
            }

            continue;
        }

        IpcMessage response = {0};
        response.type = MSG_ACCEPT;

        if (send_message(write_fd, &response) == -1) {
            perror("Child: send ACCEPT");
            close(copy_fd);
            return 1;
        }

        off_t remaining = message.file_size;
        int write_failed = 0;
        int saved_errno = 0;

        while (remaining > 0) {
            size_t chunk_size = remaining > (off_t)sizeof(buffer)
                ? sizeof(buffer)
                : (size_t)remaining;

            ssize_t bytes_read = read(read_fd, buffer, chunk_size);

            if (bytes_read == -1 && errno == EINTR) {
                continue;
            }

            if (bytes_read <= 0) {
                perror("Child: receive file data");
                close(copy_fd);
                return 1;
            }

            if (!write_failed &&
                filecopy_write_all(
                    copy_fd, buffer, (size_t)bytes_read) == -1) {
                write_failed = 1;
                saved_errno = errno;
            }

            remaining -= (off_t)bytes_read;
        }

        if (close(copy_fd) == -1 && !write_failed) {
            write_failed = 1;
            saved_errno = errno;
        }

        response = (IpcMessage){0};

        if (write_failed) {
            response.type = MSG_ERROR;
            response.error_code = saved_errno;
        } else {
            response.type = MSG_DONE;
        }

        if (send_message(write_fd, &response) == -1) {
            perror("Child: send result");
            return 1;
        }
    }
}

static int parent_process(int write_fd, int read_fd, int file_count, char *const file_paths[]) {
    char buffer[TRANSFER_BLOCK_SIZE];

    for (int i = 0; i < file_count; i++) {
        IpcMessage message = {0};

        if (receive_message(read_fd, &message) == -1) {
            perror("Parent: receive READY");
            return -1;
        }

        if (message.type != MSG_READY) {
            fprintf(stderr, "Parent: expected READY\n");
            return -1;
        }

        const char *path = file_paths[i];

        if (strlen(path) >= sizeof(message.filename)) {
            fprintf(stderr, "File path is too long: %s\n", path);

            message = (IpcMessage){0};
            message.type = MSG_SKIP;

            if (send_message(write_fd, &message) == -1) {
                perror("Parent: send SKIP");
                return -1;
            }

            continue;
        }

        int source_fd;
        off_t file_size;

        if (filecopy_open_source(path, &source_fd, &file_size) == -1) {
            fprintf(stderr, "Cannot open '%s': %s\n",
                    path, strerror(errno));

            message = (IpcMessage){0};
            message.type = MSG_SKIP;

            if (send_message(write_fd, &message) == -1) {
                perror("Parent: send SKIP");
                return -1;
            }

            continue;
        }

        message = (IpcMessage){0};
        message.type = MSG_FILE;
        message.file_size = file_size;
        memcpy(message.filename, path, strlen(path) + 1);

        if (send_message(write_fd, &message) == -1) {
            perror("Parent: send file information");
            close(source_fd);
            return -1;
        }

        IpcMessage response;

        if (receive_message(read_fd, &response) == -1) {
            perror("Parent: receive file response");
            close(source_fd);
            return -1;
        }

        if (response.type == MSG_ERROR) {
            fprintf(stderr, "Cannot create copy of '%s': %s\n",
                    path, strerror(response.error_code));
            close(source_fd);
            continue;
        }

        if (response.type != MSG_ACCEPT) {
            fprintf(stderr, "Parent: expected ACCEPT\n");
            close(source_fd);
            return -1;
        }

        off_t remaining = file_size;

        while (remaining > 0) {
            size_t chunk_size = remaining > (off_t)sizeof(buffer)
                ? sizeof(buffer)
                : (size_t)remaining;

            ssize_t bytes_read = read(source_fd, buffer, chunk_size);

            if (bytes_read == -1 && errno == EINTR) {
                continue;
            }

            if (bytes_read <= 0) {
                if (bytes_read == 0) {
                    errno = EIO;
                }

                perror("Parent: read source file");
                close(source_fd);
                return -1;
            }

            if (write_all(write_fd, buffer,
                          (size_t)bytes_read) == -1) {
                perror("Parent: send file data");
                close(source_fd);
                return -1;
            }

            remaining -= (off_t)bytes_read;
        }

        if (close(source_fd) == -1) {
            perror("Parent: close source file");
            return -1;
        }

        if (receive_message(read_fd, &response) == -1) {
            perror("Parent: receive copy result");
            return -1;
        }

        if (response.type == MSG_ERROR) {
            fprintf(stderr, "Error writing copy of '%s': %s\n",
                    path, strerror(response.error_code));
        } else if (response.type != MSG_DONE) {
            fprintf(stderr, "Parent: expected DONE\n");
            return -1;
        } else {
            printf("Copied: %s -> %s.copy\n", path, path);
        }
    }

    IpcMessage message = {0};

    if (receive_message(read_fd, &message) == -1) {
        perror("Parent: receive final READY");
        return -1;
    }

    if (message.type != MSG_READY) {
        fprintf(stderr, "Parent: expected final READY\n");
        return -1;
    }

    message = (IpcMessage){0};
    message.type = MSG_END;

    if (send_message(write_fd, &message) == -1) {
        perror("Parent: send END");
        return -1;
    }

    return 0;
}

int run_file_copy(int file_count, char *const file_paths[], const char *fifo_path) {
    int parent_to_child[2] = {-1, -1};
    int child_to_parent[2] = {-1, -1};

    char reply_fifo[FILECOPY_PATH_MAX + sizeof(".reply")];
    int fifo_mode = (fifo_path != NULL);

    if (fifo_mode) {
        if (fifo_path[0] == '\0' ||
            strlen(fifo_path) >= FILECOPY_PATH_MAX) {
            fprintf(stderr, "Invalid FIFO path\n");
            return -1;
        }

        int length = snprintf(
            reply_fifo,
            sizeof(reply_fifo),
            "%s.reply",
            fifo_path
        );

        if (length < 0 || (size_t)length >= sizeof(reply_fifo)) {
            fprintf(stderr, "Reply FIFO path is too long\n");
            return -1;
        }

        if (mkfifo(fifo_path, 0600) == -1) {
            perror("mkfifo data");
            return -1;
        }

        if (mkfifo(reply_fifo, 0600) == -1) {
            int saved_errno = errno;
            unlink(fifo_path);
            errno = saved_errno;
            perror("mkfifo reply");
            return -1;
        }
    } else {
        if (pipe(parent_to_child) == -1) {
            perror("pipe parent_to_child");
            return -1;
        }

        if (pipe(child_to_parent) == -1) {
            perror("pipe child_to_parent");
            close(parent_to_child[0]);
            close(parent_to_child[1]);
            return -1;
        }
    }

    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("sigaction SIGPIPE");

        if (!fifo_mode) {
            close(parent_to_child[0]);
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            close(child_to_parent[1]);
        } else {
            unlink(fifo_path);
            unlink(reply_fifo);
        }

        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");

        if (!fifo_mode) {
            close(parent_to_child[0]);
            close(parent_to_child[1]);
            close(child_to_parent[0]);
            close(child_to_parent[1]);
        } else {
            unlink(fifo_path);
            unlink(reply_fifo);
        }

        return -1;
    }

    if (pid == 0) {
        int read_fd;
        int write_fd;

        if (fifo_mode) {
            read_fd = open(fifo_path, O_RDONLY);

            if (read_fd == -1) {
                perror("Child: open data FIFO");
                _exit(1);
            }

            write_fd = open(reply_fifo, O_WRONLY);

            if (write_fd == -1) {
                perror("Child: open reply FIFO");
                close(read_fd);
                _exit(1);
            }
        } else {
            close(parent_to_child[1]);
            close(child_to_parent[0]);

            read_fd = parent_to_child[0];
            write_fd = child_to_parent[1];
        }

        int result = child_process(read_fd, write_fd);

        close(read_fd);
        close(write_fd);

        _exit(result);
    }

    int write_fd;
    int read_fd;

    if (fifo_mode) {
        write_fd = open(fifo_path, O_WRONLY);

        if (write_fd == -1) {
            perror("Parent: open data FIFO");
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            unlink(fifo_path);
            unlink(reply_fifo);
            return -1;
        }

        read_fd = open(reply_fifo, O_RDONLY);

        if (read_fd == -1) {
            perror("Parent: open reply FIFO");
            close(write_fd);
            kill(pid, SIGTERM);
            waitpid(pid, NULL, 0);
            unlink(fifo_path);
            unlink(reply_fifo);
            return -1;
        }
    } else {
        close(parent_to_child[0]);
        close(child_to_parent[1]);

        write_fd = parent_to_child[1];
        read_fd = child_to_parent[0];
    }

    int result = parent_process(
        write_fd,
        read_fd,
        file_count,
        file_paths
    );

    close(write_fd);
    close(read_fd);

    int status;

    if (waitpid(pid, &status, 0) == -1) {
        perror("waitpid");
        result = -1;
    } else if (!WIFEXITED(status) ||
               WEXITSTATUS(status) != 0) {
        fprintf(stderr, "Child process failed\n");
        result = -1;
    }

    if (fifo_mode) {
        if (unlink(fifo_path) == -1) {
            perror("unlink data FIFO");
            result = -1;
        }

        if (unlink(reply_fifo) == -1) {
            perror("unlink reply FIFO");
            result = -1;
        }
    }

    return result;
}