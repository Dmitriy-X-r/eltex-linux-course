#include "shared.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static size_t align_up(size_t value, size_t alignment) {
    return ((value + alignment - 1) / alignment) * alignment;
}

int create_shared_resources(int *shmid, int *semid, SharedHeader **shared) {
    *shmid = shmget(SHM_KEY, SHM_SIZE,
                    IPC_CREAT | IPC_EXCL | 0666);

    if (*shmid == -1) {
        perror("shmget");
        fprintf(stderr,
                "Возможно, остался старый сегмент памяти.\n");
        return -1;
    }

    *semid = semget(SEM_KEY, 1,
                    IPC_CREAT | IPC_EXCL | 0666);

    if (*semid == -1) {
        perror("semget");
        shmctl(*shmid, IPC_RMID, NULL);
        return -1;
    }

    union semun argument;
    argument.val = 1;

    if (semctl(*semid, 0, SETVAL, argument) == -1) {
        perror("semctl SETVAL");
        semctl(*semid, 0, IPC_RMID);
        shmctl(*shmid, IPC_RMID, NULL);
        return -1;
    }

    *shared = shmat(*shmid, NULL, 0);

    if (*shared == (void *)-1) {
        perror("shmat");
        semctl(*semid, 0, IPC_RMID);
        shmctl(*shmid, IPC_RMID, NULL);
        return -1;
    }

    memset(*shared, 0, SHM_SIZE);

    (*shared)->used_bytes =
        align_up(sizeof(SharedHeader), _Alignof(DataBlock));

    return 0;
}

int attach_shared_resources(int *shmid, int *semid, SharedHeader **shared) {
    *shmid = shmget(SHM_KEY, SHM_SIZE, 0666);

    if (*shmid == -1) {
        perror("shmget");
        return -1;
    }

    *semid = semget(SEM_KEY, 1, 0666);

    if (*semid == -1) {
        perror("semget");
        return -1;
    }

    *shared = shmat(*shmid, NULL, 0);

    if (*shared == (void *)-1) {
        perror("shmat");
        return -1;
    }

    return 0;
}

static int semaphore_operation(int semid, short operation) {
    struct sembuf operation_data = {
        .sem_num = 0,
        .sem_op = operation,
        .sem_flg = 0
    };

    while (semop(semid, &operation_data, 1) == -1) {
        if (errno == EINTR) {
            continue;
        }

        return -1;
    }

    return 0;
}

int semaphore_lock(int semid) {
    return semaphore_operation(semid, -1);
}

int semaphore_unlock(int semid) {
    return semaphore_operation(semid, 1);
}

void destroy_shared_resources(int shmid, int semid, SharedHeader *shared) {
    if (shmdt(shared) == -1) {
        perror("shmdt");
    }

    if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        perror("shmctl IPC_RMID");
    }

    if (semctl(semid, 0, IPC_RMID) == -1) {
        perror("semctl IPC_RMID");
    }
}
