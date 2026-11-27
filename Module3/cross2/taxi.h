#ifndef TAXI_H
#define TAXI_H

#include <stdint.h>
#include <sys/types.h>

#define MAX_DRIVERS 64
#define MAX_TASK_SECONDS 86400
#define INPUT_LENGTH 512

typedef enum {
    CMD_SEND_TASK,
    CMD_GET_STATUS,
    CMD_SHUTDOWN
} CommandType;

typedef enum {
    RESULT_OK,
    RESULT_BUSY,
    RESULT_AVAILABLE,
    RESULT_ERROR
} ResultType;

typedef struct {
    CommandType command;
    uint64_t request_id;
    int task_seconds;
} DriverCommand;

typedef struct {
    CommandType command;
    ResultType result;
    uint64_t request_id;
    pid_t pid;
    int remaining_seconds;
} DriverResponse;

int run_taxi(void);

#endif
