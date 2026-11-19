#ifndef SHARED_H
#define SHARED_H

#include <stddef.h>
#include <semaphore.h>

#define SHM_SIZE 512

#define SHM_NAME "/eltex_pc_shm"
#define SEM_NAME "/eltex_pc_sem"

typedef struct {
    int generation_done;
    size_t first_offset;
    size_t last_offset;
    size_t used_bytes;
    size_t active_consumers;
} SharedHeader;

typedef struct {
    size_t count;
    size_t next_offset;
    int numbers[];
} DataBlock;

int create_shared_resources(int *shm_fd, sem_t **sem,
                            SharedHeader **shared);

int attach_shared_resources(int *shm_fd, sem_t **sem,
                            SharedHeader **shared);

int semaphore_lock(sem_t *sem);
int semaphore_unlock(sem_t *sem);

void destroy_shared_resources(int shm_fd, sem_t *sem,
                              SharedHeader *shared);

int producer_run(void);
int consumer_run(void);

#endif
