
#include "shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define MAX_NUMBERS_PER_BLOCK 5
#define MIN_NUMBERS_PER_BLOCK 1

static volatile sig_atomic_t stop_requested = 0;

static void handle_signal(int signal_number)
{
    (void)signal_number;
    stop_requested = 1;
}

static size_t align_up(size_t value, size_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

static size_t block_size_for(size_t count)
{
    size_t raw_size = offsetof(DataBlock, numbers)
                    + count * sizeof(int);

    return align_up(raw_size, _Alignof(DataBlock));
}

static int append_block(SharedHeader *shared,
                        const int *numbers,
                        size_t count,
                        size_t *offset_out)
{
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

static int all_blocks_processed(const SharedHeader *shared)
{
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

static int wait_for_consumers_to_detach(sem_t *sem,
                                        SharedHeader *shared)
{
    printf("[Производитель] Ожидание завершения потребителей...\n");

    while (1) {
        if (semaphore_lock(sem) == -1) {
            perror("sem_wait");
            return -1;
        }

        size_t active = shared->active_consumers;

        if (semaphore_unlock(sem) == -1) {
            perror("sem_post");
            return -1;
        }

        if (active == 0) {
            return 0;
        }

        sleep(1);
    }
}

int producer_run(void)
{
    int shm_fd;
    sem_t *sem;
    SharedHeader *shared;

    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    if (signal(SIGTERM, handle_signal) == SIG_ERR) {
        perror("signal");
        return 1;
    }

    if (create_shared_resources(&shm_fd, &sem, &shared) == -1) {
        return 1;
    }

    srand((unsigned int)(time(NULL) ^ getpid()));

    printf("[Производитель] Запущен. Размер памяти: %d байт.\n",
           SHM_SIZE);
    fflush(stdout);

    int result = 0;

    while (!stop_requested) {
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

        if (semaphore_lock(sem) == -1) {
            perror("sem_wait");
            result = 1;
            break;
        }

        if (stop_requested) {
            shared->generation_done = 1;
            generation_finished = 1;
        } else if (!shared->generation_done) {
            appended = append_block(shared, numbers,
                                    count, &offset);

            if (!appended) {
                shared->generation_done = 1;
                generation_finished = 1;
            }
        }

        done = shared->generation_done
            && all_blocks_processed(shared);

        if (semaphore_unlock(sem) == -1) {
            perror("sem_post");
            result = 1;
            break;
        }

        if (appended) {
            printf("[Производитель] Блок %zu: ", offset);

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

        sleep(1);
    }

    if (result == 0) {
        printf("[Производитель] Все наборы обработаны.\n");
    }

    if (wait_for_consumers_to_detach(sem, shared) == -1) {
        result = 1;
    }

    destroy_shared_resources(shm_fd, sem, shared);

    printf("[Производитель] POSIX-ресурсы удалены.\n");

    return result;
}
