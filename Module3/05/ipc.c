
#include "shared.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static size_t align_up(size_t value, size_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

int create_shared_resources(int *shm_fd, sem_t **sem,
                            SharedHeader **shared)
{
    *shm_fd = shm_open(SHM_NAME,
                       O_CREAT | O_EXCL | O_RDWR,
                       0666);

    if (*shm_fd == -1) {
        perror("shm_open");
        fprintf(stderr,
                "Возможно, остался старый объект разделяемой памяти.\n");
        return -1;
    }

    if (ftruncate(*shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        close(*shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    *shared = mmap(NULL, SHM_SIZE,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, *shm_fd, 0);

    if (*shared == MAP_FAILED) {
        perror("mmap");
        close(*shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    *sem = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 1);

    if (*sem == SEM_FAILED) {
        perror("sem_open");
        munmap(*shared, SHM_SIZE);
        close(*shm_fd);
        shm_unlink(SHM_NAME);
        return -1;
    }

    memset(*shared, 0, SHM_SIZE);

    (*shared)->used_bytes =
        align_up(sizeof(SharedHeader), _Alignof(DataBlock));

    return 0;
}

int attach_shared_resources(int *shm_fd, sem_t **sem,
                            SharedHeader **shared)
{
    *shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);

    if (*shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    *shared = mmap(NULL, SHM_SIZE,
                   PROT_READ | PROT_WRITE,
                   MAP_SHARED, *shm_fd, 0);

    if (*shared == MAP_FAILED) {
        perror("mmap");
        close(*shm_fd);
        return -1;
    }

    *sem = sem_open(SEM_NAME, 0);

    if (*sem == SEM_FAILED) {
        perror("sem_open");
        munmap(*shared, SHM_SIZE);
        close(*shm_fd);
        return -1;
    }

    return 0;
}

int semaphore_lock(sem_t *sem)
{
    while (sem_wait(sem) == -1) {
        if (errno == EINTR) {
            continue;
        }

        return -1;
    }

    return 0;
}

int semaphore_unlock(sem_t *sem)
{
    return sem_post(sem);
}

void destroy_shared_resources(int shm_fd, sem_t *sem,
                              SharedHeader *shared)
{
    if (munmap(shared, SHM_SIZE) == -1) {
        perror("munmap");
    }

    if (close(shm_fd) == -1) {
        perror("close");
    }

    if (sem_close(sem) == -1) {
        perror("sem_close");
    }

    if (shm_unlink(SHM_NAME) == -1) {
        perror("shm_unlink");
    }

    if (sem_unlink(SEM_NAME) == -1) {
        perror("sem_unlink");
    }
}
