#define _GNU_SOURCE

#include "driver.h"
#include "taxi.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

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

static int read_full(int fd, void *buffer, size_t size){
    char *data = buffer;
    size_t received = 0;

    while (received < size) {
        ssize_t result = read(fd, data + received, size - received);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (result == 0) {
            return received == 0 ? 0 : -1;
        }

        received += (size_t)result;
    }

    return 1;
}

static int get_remaining_seconds(int timer_fd){
    struct itimerspec timer;

    if (timerfd_gettime(timer_fd, &timer) == -1) {
        return -1;
    }

    if (timer.it_value.tv_sec == 0 &&
        timer.it_value.tv_nsec == 0) {
        return 0;
    }

    int remaining = (int)timer.it_value.tv_sec;

    if (timer.it_value.tv_nsec > 0) {
        remaining++;
    }

    return remaining;
}

static int start_task_timer(int timer_fd, int seconds){
    struct itimerspec timer = {0};

    timer.it_value.tv_sec = seconds;

    return timerfd_settime(timer_fd, 0, &timer, NULL);
}

static int send_response(
    int response_fd,
    const DriverCommand *command,
    ResultType result,
    int remaining_seconds)
{
    DriverResponse response = {
        .command = command->command,
        .result = result,
        .request_id = command->request_id,
        .pid = getpid(),
        .remaining_seconds = remaining_seconds
    };

    return write_full(response_fd, &response, sizeof(response));
}

int driver_run(int command_fd, int response_fd){
    int timer_fd = timerfd_create(
        CLOCK_MONOTONIC,
        TFD_NONBLOCK | TFD_CLOEXEC
    );

    if (timer_fd == -1) {
        perror("timerfd_create");
        close(command_fd);
        close(response_fd);
        return -1;
    }

    int busy = 0;
    int running = 1;

    while (running) {
        fd_set read_fds;

        FD_ZERO(&read_fds);
        FD_SET(command_fd, &read_fds);
        FD_SET(timer_fd, &read_fds);

        int max_fd = command_fd > timer_fd
            ? command_fd
            : timer_fd;

        int ready = select(max_fd + 1, &read_fds,
                           NULL, NULL, NULL);

        if (ready == -1) {
            if (errno == EINTR) {
                continue;
            }

            perror("driver select");
            break;
        }

        /*
         * Сначала обрабатываем таймер.
         * Если он истёк, водитель уже Available.
         */
        if (FD_ISSET(timer_fd, &read_fds)) {
            uint64_t expirations;

            ssize_t result = read(
                timer_fd,
                &expirations,
                sizeof(expirations)
            );

            if (result == (ssize_t)sizeof(expirations)) {
                busy = 0;
            } else if (result == -1 &&
                       errno != EAGAIN &&
                       errno != EINTR) {
                perror("timerfd read");
                break;
            }
        }

        if (FD_ISSET(command_fd, &read_fds)) {
            DriverCommand command;

            int result = read_full(
                command_fd,
                &command,
                sizeof(command)
            );

            if (result <= 0) {
                running = 0;
                continue;
            }

            if (command.command == CMD_SHUTDOWN) {
                running = 0;
                continue;
            }

            if (command.command == CMD_GET_STATUS) {
                int remaining = busy
                    ? get_remaining_seconds(timer_fd)
                    : 0;

                if (remaining < 0) {
                    perror("timerfd_gettime");
                    break;
                }

                ResultType status = remaining > 0
                    ? RESULT_BUSY
                    : RESULT_AVAILABLE;

                if (send_response(
                        response_fd,
                        &command,
                        status,
                        remaining) == -1) {
                    running = 0;
                }

                continue;
            }

            if (command.command == CMD_SEND_TASK) {
                int remaining = busy
                    ? get_remaining_seconds(timer_fd)
                    : 0;

                if (remaining < 0) {
                    perror("timerfd_gettime");
                    break;
                }

                if (remaining > 0) {
                    if (send_response(
                            response_fd,
                            &command,
                            RESULT_BUSY,
                            remaining) == -1) {
                        running = 0;
                    }

                    continue;
                }

                if (start_task_timer(
                        timer_fd,
                        command.task_seconds) == -1) {
                    perror("timerfd_settime");

                    if (send_response(
                            response_fd,
                            &command,
                            RESULT_ERROR,
                            0) == -1) {
                        running = 0;
                    }

                    continue;
                }

                busy = 1;

                if (send_response(
                        response_fd,
                        &command,
                        RESULT_OK,
                        command.task_seconds) == -1) {
                    running = 0;
                }
            }
        }
    }

    close(timer_fd);
    close(command_fd);
    close(response_fd);

    return 0;
}
