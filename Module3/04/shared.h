#ifndef SHARED_H
#define SHARED_H

#include <stddef.h>
#include <sys/types.h>

#define SHM_SIZE 512

#define SHM_KEY ((key_t)0x45A31)
#define SEM_KEY ((key_t)0x45A32)

typedef struct {
    int generation_done;
    size_t first_offset;
    size_t last_offset;
    size_t used_bytes;
} SharedHeader;

typedef struct {
    size_t count;
    size_t next_offset;
    int numbers[];
} DataBlock;

int create_shared_resources(int *shmid, int *semid, SharedHeader **shared);
int attach_shared_resources(int *shmid, int *semid, SharedHeader **shared);

int semaphore_lock(int semid);
int semaphore_unlock(int semid);

void destroy_shared_resources(int shmid, int semid, SharedHeader *shared);

int producer_run(void);
int consumer_run(void);

#endif
