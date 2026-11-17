#include "shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/shm.h>

#define MAX_NUMBERS_PER_BLOCK 5
#define MIN_NUMBERS_PER_BLOCK 1

static size_t align_up(size_t value, size_t alignment) {
    return ((value + alignment - 1) / alignment) * alignment;
}

static size_t block_size_for(size_t count) {
    size_t raw_size = offsetof(DataBlock, numbers)
                    + count * sizeof(int);

    return align_up(raw_size, _Alignof(DataBlock));
}

static int append_block(SharedHeader *shared, const int *numbers, size_t count, size_t *offset_out) {
    size_t block_size = block_size_for(count);

    if (shared->used_bytes > SHM_SIZE
        || block_size > SHM_SIZE - shared->used_bytes) {
        return 0;
    }

    size_t offset = shared->used_bytes;

    DataBlock *block =
        (DataBlock *)((char *)shared + offset);

    block->count = count;
    block->next_offset = 0;

    for (size_t i = 0; i < count; i++) {
        block->numbers[i] = numbers[i];
    }

    if (shared->last_offset != 0) {
        DataBlock *last =
            (DataBlock *)((char *)shared
                          + shared->last_offset);

        last->next_offset = offset;
    } else {
        shared->first_offset = offset;
    }

    shared->last_offset = offset;
    shared->used_bytes += block_size;
    *offset_out = offset;

    return 1;
}

static int all_blocks_processed(const SharedHeader *shared) {
    size_t offset = shared->first_offset;

    while (offset != 0) {
        const DataBlock *block =
            (const DataBlock *)((const char *)shared + offset);

        if (block->count != 0) {
            return 0;
        }

        offset = block->next_offset;
    }

    return 1;
}

static int wait_for_consumer(int shmid)
{
    struct shmid_ds info;

    printf("Ожидание подключения потребителя...\n");

    while (1) {
        if (shmctl(shmid, IPC_STAT, &info) == -1) {
            perror("shmctl IPC_STAT");
            return -1;
        }

        if (info.shm_nattch >= 2) {
            printf("Потребитель подключился. Начинаем генерацию.\n");
            return 0;
        }

        sleep(1);
    }
}

static int wait_for_consumers_to_detach(int shmid)
{
    struct shmid_ds info;

    printf("Ожидание завершения потребителей...\n");

    while (1) {
        if (shmctl(shmid, IPC_STAT, &info) == -1) {
            perror("shmctl IPC_STAT");
            return -1;
        }

        if (info.shm_nattch == 1) {
            return 0;
        }

        sleep(1);
    }
}

int producer_run(void) {
    int shmid;
    int semid;
    SharedHeader *shared;

    if (create_shared_resources(&shmid, &semid, &shared) == -1) {
        return 1;
    }

    if (wait_for_consumer(shmid) == -1) {
        destroy_shared_resources(shmid, semid, shared);
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));

    printf("Производитель запущен. Размер памяти: %d байт.\n",
           SHM_SIZE);
    printf("Теперь запустите потребителя в другом терминале.\n");
    fflush(stdout);

    int result = 0;

    while (1) {
        int numbers[MAX_NUMBERS_PER_BLOCK];
        size_t count = MIN_NUMBERS_PER_BLOCK
                     + (size_t)(rand() % MAX_NUMBERS_PER_BLOCK);
        size_t offset = 0;
        int appended = 0;
        int generation_finished = 0;
        int done = 0;

        for (size_t i = 0; i < count; i++) {
            numbers[i] = rand() % 101 - 50;
        }

        if (semaphore_lock(semid) == -1) {
            perror("semop lock");
            result = 1;
            break;
        }

        if (!shared->generation_done) {
            appended = append_block(shared, numbers,
                                    count, &offset);

            if (!appended) {
                shared->generation_done = 1;
                generation_finished = 1;
            }
        }

        done = shared->generation_done
            && all_blocks_processed(shared);

        if (semaphore_unlock(semid) == -1) {
            perror("semop unlock");
            result = 1;
            break;
        }

        if (appended) {
            printf("[Производитель] Блок %zu: ",
                   offset);

            for (size_t i = 0; i < count; i++) {
                printf("%d ", numbers[i]);
            }

            printf("\n");
        }

        if (generation_finished) {
            printf("[Производитель] Память заполнена. "
                   "Генерация завершена.\n");
        }

        if (done) {
            break;
        }

        // if (shared->generation_done) {
        //     sleep(1);
        // } else {
        //     usleep(300000);
        // }
        sleep(1);
    }

    if (result == 0) {
        printf("[Производитель] Все наборы обработаны.\n");
    }

    if (wait_for_consumers_to_detach(shmid) == -1) {
        return 1;
    }

    destroy_shared_resources(shmid, semid, shared);

    printf("[Производитель] IPC-ресурсы удалены.\n");

    return result;
}
