#include "shared.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/shm.h>

#define MAX_NUMBERS_PER_BLOCK 5

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

int consumer_run(void) {
    int shmid;
    int semid;
    SharedHeader *shared;

    if (attach_shared_resources(&shmid, &semid, &shared) == -1) {
        return 1;
    }

    printf("[Потребитель] Подключён к общей памяти.\n");

    int result = 0;

    while (1) {
        int numbers[MAX_NUMBERS_PER_BLOCK];
        size_t count = 0;
        size_t found_offset = 0;
        int minimum = 0;
        int maximum = 0;
        int found = 0;
        int finished = 0;

        if (semaphore_lock(semid) == -1) {
            if (errno == EIDRM || errno == EINVAL) {
                break;
            }

            perror("semop lock");
            result = 1;
            break;
        }

        size_t offset = shared->first_offset;

        while (offset != 0) {
            DataBlock *block =
                (DataBlock *)((char *)shared + offset);

            if (block->count != 0) {
                count = block->count;
                found_offset = offset;

                for (size_t i = 0; i < count; i++) {
                    numbers[i] = block->numbers[i];
                }

                minimum = numbers[0];
                maximum = numbers[0];

                for (size_t i = 1; i < count; i++) {
                    if (numbers[i] < minimum) {
                        minimum = numbers[i];
                    }

                    if (numbers[i] > maximum) {
                        maximum = numbers[i];
                    }
                }

                block->count = 0;
                found = 1;
                break;
            }

            offset = block->next_offset;
        }

        finished = shared->generation_done && all_blocks_processed(shared);

        if (semaphore_unlock(semid) == -1) {
            perror("semop unlock");
            result = 1;
            break;
        }

        if (found) {
            printf("[Потребитель PID %ld] "
                   "Блок %zu: ",
                   (long)getpid(), found_offset);

            for (size_t i = 0; i < count; i++) {
                printf("%d ", numbers[i]);
            }

            printf("| min = %d, max = %d\n",
                   minimum, maximum);
        }

        if (finished) {
            printf("[Потребитель PID %ld] "
                   "Все наборы обработаны. Завершаюсь.\n",
                   (long)getpid());
            break;
        }

        sleep(1);
    }

    if (shmdt(shared) == -1) {
        perror("shmdt");
        result = 1;
    }

    return result;
}
