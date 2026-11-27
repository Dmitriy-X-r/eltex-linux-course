#define _POSIX_C_SOURCE 200809L

#include "taxi.h"
#include "driver.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    pid_t pid;
    int command_fd;
    int response_fd;
    int alive;
} DriverInfo;

static DriverInfo drivers[MAX_DRIVERS];
static size_t driver_count;
static uint64_t next_request_id = 1;

static int write_full(int fd, const void *buffer, size_t size){
    const char *data = buffer;
    size_t written = 0;

    while (written < size) {
        ssize_t result = write(fd, data + written, size - written);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (result == 0) {
            return -1;
        }

        written += (size_t)result;
    }

    return 0;
}

static int find_driver(pid_t pid){
    for (size_t i = 0; i < driver_count; i++) {
        if (drivers[i].pid == pid && drivers[i].alive) {
            return (int)i;
        }
    }

    return -1;
}

static void mark_driver_dead(size_t index){
    if (!drivers[index].alive) {
        return;
    }

    close(drivers[index].command_fd);
    close(drivers[index].response_fd);

    drivers[index].command_fd = -1;
    drivers[index].response_fd = -1;
    drivers[index].alive = 0;

    fprintf(stderr, "Driver %ld disconnected\n",
            (long)drivers[index].pid);
}

static int create_driver(void){
    if (driver_count >= MAX_DRIVERS) {
        fprintf(stderr, "Maximum number of drivers reached\n");
        return -1;
    }

    int command_pipe[2];
    int response_pipe[2];

    if (pipe(command_pipe) == -1) {
        perror("pipe");
        return -1;
    }

    if (pipe(response_pipe) == -1) {
        perror("pipe");

        close(command_pipe[0]);
        close(command_pipe[1]);

        return -1;
    }

    /*
     * select() использует fd_set, поэтому дескрипторы
     * должны быть меньше FD_SETSIZE.
     */
    if (command_pipe[0] >= FD_SETSIZE ||
        command_pipe[1] >= FD_SETSIZE ||
        response_pipe[0] >= FD_SETSIZE ||
        response_pipe[1] >= FD_SETSIZE) {
        fprintf(stderr, "File descriptor limit for select reached\n");

        close(command_pipe[0]);
        close(command_pipe[1]);
        close(response_pipe[0]);
        close(response_pipe[1]);

        return -1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");

        close(command_pipe[0]);
        close(command_pipe[1]);
        close(response_pipe[0]);
        close(response_pipe[1]);

        return -1;
    }

    if (pid == 0) {
        close(command_pipe[1]);
        close(response_pipe[0]);

        /*
         * Закрываем унаследованные концы каналов
         * ранее созданных водителей.
         */
        for (size_t i = 0; i < driver_count; i++) {
            if (drivers[i].command_fd >= 0) {
                close(drivers[i].command_fd);
            }

            if (drivers[i].response_fd >= 0) {
                close(drivers[i].response_fd);
            }
        }

        int result = driver_run(
            command_pipe[0],
            response_pipe[1]
        );

        _exit(result == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    close(command_pipe[0]);
    close(response_pipe[1]);

    drivers[driver_count].pid = pid;
    drivers[driver_count].command_fd = command_pipe[1];
    drivers[driver_count].response_fd = response_pipe[0];
    drivers[driver_count].alive = 1;

    driver_count++;

    printf("Driver created. PID: %ld\n", (long)pid);

    return 0;
}

static int send_command(
    size_t index,
    CommandType command_type,
    int task_seconds,
    uint64_t request_id)
{
    DriverCommand command = {
        .command = command_type,
        .request_id = request_id,
        .task_seconds = task_seconds
    };

    if (write_full(
            drivers[index].command_fd,
            &command,
            sizeof(command)) == -1) {
        perror("write command");
        mark_driver_dead(index);
        return -1;
    }

    return 0;
}

static void print_response(const DriverResponse *response){
    if (response->command == CMD_GET_STATUS) {
        if (response->result == RESULT_BUSY) {
            printf("%ld Busy %d\n",
                   (long)response->pid,
                   response->remaining_seconds);
        } else if (response->result == RESULT_AVAILABLE) {
            printf("%ld Available\n", (long)response->pid);
        } else {
            printf("%ld Error\n", (long)response->pid);
        }

        return;
    }

    if (response->command == CMD_SEND_TASK) {
        if (response->result == RESULT_OK) {
            printf("Task accepted by driver %ld\n",
                   (long)response->pid);
        } else if (response->result == RESULT_BUSY) {
            printf("Busy %d\n", response->remaining_seconds);
        } else {
            printf("Driver %ld: task failed\n",
                   (long)response->pid);
        }
    }
}

static int parse_pid(const char *text, pid_t *pid){
    char *end = NULL;
    errno = 0;

    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value <= 0) {
        return -1;
    }

    *pid = (pid_t)value;

    return 0;
}

static int parse_seconds(const char *text, int *seconds){
    char *end = NULL;
    errno = 0;

    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value <= 0 || value > MAX_TASK_SECONDS) {
        return -1;
    }

    *seconds = (int)value;

    return 0;
}

static void print_help(void){
    printf("Commands:\n");
    printf("  create_driver\n");
    printf("  send_task <pid> <task_timer>\n");
    printf("  get_status <pid>\n");
    printf("  get_drivers\n");
    printf("  quit\n");
}

static int process_command(char *line){
    char *tokens[4];
    size_t count = 0;

    char *saveptr = NULL;
    char *token = strtok_r(line, " \t\r\n", &saveptr);

    while (token != NULL) {
        if (count == 4) {
            fprintf(stderr, "Too many arguments\n");
            return 0;
        }

        tokens[count++] = token;
        token = strtok_r(NULL, " \t\r\n", &saveptr);
    }

    if (count == 0) {
        return 0;
    }

    if (strcmp(tokens[0], "quit") == 0) {
        if (count != 1) {
            fprintf(stderr, "Usage: quit\n");
            return 0;
        }

        return 1;
    }

    if (strcmp(tokens[0], "help") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(tokens[0], "create_driver") == 0) {
        if (count != 1) {
            fprintf(stderr, "Usage: create_driver\n");
            return 0;
        }

        create_driver();
        return 0;
    }

    if (strcmp(tokens[0], "send_task") == 0 ||
        strcmp(tokens[0], "get_status") == 0) {
        int is_task = strcmp(tokens[0], "send_task") == 0;

        if (count != (is_task ? 3U : 2U)) {
            fprintf(stderr, "Invalid number of arguments\n");
            return 0;
        }

        pid_t pid;

        if (parse_pid(tokens[1], &pid) == -1) {
            fprintf(stderr, "Invalid PID\n");
            return 0;
        }

        int index = find_driver(pid);

        if (index == -1) {
            fprintf(stderr, "Unknown driver PID: %ld\n", (long)pid);
            return 0;
        }

        int seconds = 0;

        if (is_task &&
            parse_seconds(tokens[2], &seconds) == -1) {
            fprintf(stderr,
                    "Task time must be between 1 and %d seconds\n",
                    MAX_TASK_SECONDS);
            return 0;
        }

        CommandType type = is_task
            ? CMD_SEND_TASK
            : CMD_GET_STATUS;

        uint64_t request_id = next_request_id++;

        send_command(
            (size_t)index,
            type,
            seconds,
            request_id
        );

        return 0;
    }

    if (strcmp(tokens[0], "get_drivers") == 0) {
        if (count != 1) {
            fprintf(stderr, "Usage: get_drivers\n");
            return 0;
        }

        for (size_t i = 0; i < driver_count; i++) {
            if (drivers[i].alive) {
                send_command(
                    i,
                    CMD_GET_STATUS,
                    0,
                    next_request_id
                );
            }
        }

        next_request_id++;

        return 0;
    }

    fprintf(stderr, "Unknown command. Type help\n");

    return 0;
}

static void shutdown_drivers(void){
    for (size_t i = 0; i < driver_count; i++) {
        if (drivers[i].alive) {
            DriverCommand command = {
                .command = CMD_SHUTDOWN,
                .request_id = next_request_id++,
                .task_seconds = 0
            };

            if (write_full(
                    drivers[i].command_fd,
                    &command,
                    sizeof(command)) == -1) {
                /* Процесс мог завершиться раньше. */
            }

            close(drivers[i].command_fd);
            close(drivers[i].response_fd);

            drivers[i].command_fd = -1;
            drivers[i].response_fd = -1;
            drivers[i].alive = 0;
        }
    }

    for (size_t i = 0; i < driver_count; i++) {
        while (waitpid(drivers[i].pid, NULL, 0) == -1) {
            if (errno == EINTR) {
                continue;
            }

            if (errno != ECHILD) {
                perror("waitpid");
            }

            break;
        }
    }
}

int run_taxi(void){
    /*
     * Если водитель неожиданно завершится, write() в его
     * канал не должен завершить весь диспетчер сигналом SIGPIPE.
     */
    if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        perror("signal");
        return EXIT_FAILURE;
    }

    char input[INPUT_LENGTH];
    size_t input_length = 0;
    int input_overflow = 0;
    int stdin_open = 1;
    int running = 1;

    printf("Taxi dispatcher started. Type help for commands.\n");
    printf("> ");
    fflush(stdout);

    while (running) {
        fd_set read_fds;

        FD_ZERO(&read_fds);

        int max_fd = -1;

        if (stdin_open) {
            FD_SET(STDIN_FILENO, &read_fds);
            max_fd = STDIN_FILENO;
        }

        for (size_t i = 0; i < driver_count; i++) {
            if (drivers[i].alive) {
                FD_SET(drivers[i].response_fd, &read_fds);

                if (drivers[i].response_fd > max_fd) {
                    max_fd = drivers[i].response_fd;
                }
            }
        }

        if (max_fd == -1) {
            break;
        }

        int ready = select(
            max_fd + 1,
            &read_fds,
            NULL,
            NULL,
            NULL
        );

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("dispatcher select");
            running = 0;
            break;
        }

        if (stdin_open && FD_ISSET(STDIN_FILENO, &read_fds)) {
            char ch;
            ssize_t result = read(STDIN_FILENO, &ch, 1);

            if (result == 0) {
                stdin_open = 0;
                running = 0;
            } else if (result < 0) {
                if (errno != EINTR) {
                    perror("stdin read");
                    running = 0;
                }
            } else if (ch == '\n') {
                if (input_overflow) {
                    fprintf(stderr, "Command line is too long\n");
                } else {
                    input[input_length] = '\0';

                    if (process_command(input)) {
                        running = 0;
                    }
                }

                input_length = 0;
                input_overflow = 0;

                if (running && stdin_open) {
                    printf("> ");
                    fflush(stdout);
                }
            } else if (!input_overflow) {
                if (input_length < sizeof(input) - 1) {
                    input[input_length++] = ch;
                } else {
                    input_overflow = 1;
                }
            }
        }

        if (!running) {
            break;
        }

        for (size_t i = 0; i < driver_count; i++) {
            if (!drivers[i].alive ||
                !FD_ISSET(drivers[i].response_fd, &read_fds)) {
                continue;
            }

            DriverResponse response;

            ssize_t result = read(
                drivers[i].response_fd,
                &response,
                sizeof(response)
            );

            if (result == (ssize_t)sizeof(response)) {
                print_response(&response);
                fflush(stdout);
            } else if (result == -1 && errno == EINTR) {
                continue;
            } else {
                mark_driver_dead(i);
            }
        }
    }

    shutdown_drivers();

    printf("\nTaxi dispatcher stopped.\n");

    return EXIT_SUCCESS;
}
