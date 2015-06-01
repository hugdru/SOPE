#define _XOPEN_SOURCE 700

#include <ftw.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define MAXBALCOES 32
#define MAXCLIENTES 128

/** Estruturas que serão partilhadas **/
typedef struct Info {
    /** namedPipe and Misc**/
    int namedPipeFd;
    pthread_mutex_t namedPipeMutex;
    pthread_cond_t namedPipeCondvar;
    /** End of namedPipe **/
    /** Client Contributions **/
    size_t sumatorioTempoAtendimentoClientes;
    size_t nClientesAtendidos;
    size_t tempoAbertura;
    pthread_mutex_t clientContribMutex;
    pthread_cond_t clientContribCondvar;
    /** End of Client Contributions **/
    /** Circular Fifo **/
    size_t fifo[MAXCLIENTES];
    size_t fifoCircularIndex;
    pthread_mutex_t fifoMutex;
    pthread_cond_t fifoCondvar;
    /** End of Circular Fifo **/
    /** Slots Available in Fifo **/
    size_t fifoSlots;
    pthread_mutex_t fifoSlotsMutex;
    pthread_cond_t fifoSlotsCondvar;
    /** End of Slots Available in Fifo **/
} Info_t;

typedef struct SharedMemory {
    Info_t infoBalcoes[MAXBALCOES];
    size_t nBalcoes;
    pthread_mutex_t nBalcoesMutex;
    pthread_cond_t nBalcoesCondvar;
} SharedMemory_t;
/** Fim de Estrutas que serão partilhadas **/

char *semName = NULL;
char *shmName = NULL;
size_t numeroBalcao = -1;
size_t tempoAbertura = -1;
SharedMemory_t *sharedMemory = NULL;

sem_t* getGlobalSemaphore(void);
int destroyGlobalSemaphore(sem_t *sem);
SharedMemory_t* createSharedMemory(void);
int destroySharedMemory(SharedMemory_t *shm);
SharedMemory_t* openSharedMemory(void);
int createFolder(void);
int removeTempDir(void);
int ftwHandler(const char *fpath,__attribute__((unused)) const struct stat *sb, int typeflag);
int createBalcao(void);

const char tempFilePathDir[] = "/tmp/sope/";

int main(int argc, char *argv[]) {

    int failed = 0;

    if (argc != 3) {
        fprintf(stderr, "balcao <nome_mempartilhada> <tempo_abertura>\n");
        exit(EXIT_FAILURE);
    }

    // Add the / prefix to the parameter received
    size_t shmNameSize = (strlen(argv[1]) + 1) * sizeof(argv[1][0]) + 1;
    shmName = (char *) malloc(shmNameSize);
    snprintf(shmName, shmNameSize, "%s%s", "/", argv[1]);

    // Convert from string to unsigned long
    errno = 0;
    tempoAbertura = strtoul(argv[2], NULL, 10);
    if (errno != 0) {
        fprintf(stderr, "Failure in strtoul()");
        free(shmName);
        exit(EXIT_FAILURE);
    }

    // Open or create the semaphore initialize with 1
    sem_t *globalShemaphore = getGlobalSemaphore();
    if (globalShemaphore == NULL) {
        fprintf(stderr, "Failure in globalShemaphore()\n");
        exit(EXIT_FAILURE);
    }

    // Attempt to get permission to create a semaphore
    if (sem_wait(globalShemaphore) != 0) {
        perror("Failure in sem_wait()");
        goto cleanUp;
    }
    if ((sharedMemory = openSharedMemory()) == NULL) {
        sharedMemory = createSharedMemory();
        if (sharedMemory == NULL) {
            fprintf(stderr, "Failure in createSharedMemory()\n");
            goto cleanUp;
        }

        if (createFolder() != 0) {
            perror("Failure in createFolder");
            goto cleanUp;
        }

    }
    if (sem_post(globalShemaphore) != 0) {
        perror("Failure in sem_post()");
        goto cleanUp;
    }

    if (chdir(tempFilePathDir) == -1) {
        perror("Failure in chdir()");
        goto cleanUp;
    }

    // Do this balcao stuff
    if (createBalcao() != 0) {
        perror("Failure in createNewBalcao()");
        goto cleanUp;
    }


cleanUp:
    if (destroySharedMemory(sharedMemory) != 0) {
        fprintf(stderr, "Failure in destroySharedMemory()");
        failed = 1;
    }
    if (destroyGlobalSemaphore(globalShemaphore) != 0) {
        fprintf(stderr, "Failure in destroyGlobalSemaphore()");
        failed = 1;
    }
    if (failed) EXIT_FAILURE;
    else EXIT_SUCCESS;
}

SharedMemory_t* openSharedMemory(void) {
    if (shmName == NULL) {
        errno = EINVAL;
        perror("Failure in openSharedMemory");
        return NULL;
    }

    int shmfd;
    SharedMemory_t *shm;

    shmfd = shm_open(shmName, O_RDWR, 0660);

    if (shmfd < 0) {
        perror("Failure in shm_open()");
        return NULL;
    }

    shm = (SharedMemory_t *) mmap(0, sizeof(SharedMemory_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shm == MAP_FAILED) {
        perror("Failure in mmap()");
        return NULL;
    }

    return shm;
}

SharedMemory_t* createSharedMemory(void) {

    if (shmName == NULL) {
        errno = EINVAL;
        perror("Failure in createSharedMemory");
        return NULL;
    }

    int shmfd;
    SharedMemory_t *shm;

    shmfd = shm_open(shmName, O_CREAT | O_RDWR | O_EXCL, 0660);

    if (shmfd < 0) {
        perror("Failure in shm_open()");
        return NULL;
    }

    if (ftruncate(shmfd, sizeof(SharedMemory_t)) < 0) {
        perror("Failure in ftruncate()");
        return NULL;
    }

    shm = (SharedMemory_t *) mmap(0, sizeof(SharedMemory_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (shm == MAP_FAILED) {
        perror("Failure in mmap()");
        return NULL;
    }

    // Initialize data
    shm->nBalcoes = 0;

    // Initialize the nBalcoes mutex and condvar
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&shm->nBalcoesMutex, &mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr,PTHREAD_PROCESS_SHARED);

    pthread_cond_init(&shm->nBalcoesCondvar, &cattr);

    return shm;
}


int destroySharedMemory(SharedMemory_t *shm) {

    if (shmName == NULL || shmName == NULL) {
        errno = EINVAL;
        perror("Failure in destroySharedMemory");
        return -1;
    }

    if (munmap(shm, sizeof(SharedMemory_t)) < 0) {
        perror("Failure in munmap()");
        free(shmName);
        return -1;
    }

    if (shm_unlink(shmName) < 0) {
        perror("Failure in shm_unlink()");
        return -1;
    }

    free(shmName);

    return 0;
}

sem_t* getGlobalSemaphore(void) {

    if (shmName == NULL) {
        errno = EINVAL;
        perror("Failure in createGlobalSemaphore");
        return NULL;
    }

    char const suffix[] = "Semaphore";

    size_t semNameSize = strlen(shmName) * sizeof(char) + sizeof(suffix);
    semName = (char *) malloc(semNameSize);

    snprintf(semName, semNameSize, "%s%s", shmName, suffix);

    sem_t *sem = sem_open(semName, O_CREAT, 0600, 1);
    if (sem == SEM_FAILED) {
        perror("Failure in sem_open()");
        return NULL;
    }

    return sem;
}


int destroyGlobalSemaphore(sem_t *sem) {
    if (sem == NULL) {
        errno = EINVAL;
        perror("Failure in createGlobalSemaphore");
        return -1;
    }

    int failed = 0;
    if (sem_close(sem) != 0) {
        perror("Failure in sem_close()");
        failed = 1;
    }

    if (sem_unlink(semName) != 0) {
        perror("Failure in sem_unlink");
        failed = 1;
    }
    free(semName);

    if (failed) return -1;
    return 0;
}

int createFolder(void) {
    if (mkdir(tempFilePathDir, 0770) == -1) {
        perror("Failed to create temp file folder");
        return -1;
    }

    return 0;
}

int removeTempDir(void) {

    if (ftw(tempFilePathDir, ftwHandler, 5) == -1) {
        perror("Failed to clean files inside file temp dir");
        return -1;
    }

    if (rmdir(tempFilePathDir) == -1) {
        perror("Can't delete the file temp dir");
        return -1;
    }

    return 0;
}

int ftwHandler(const char *fpath,__attribute__((unused)) const struct stat *sb, int typeflag) {

    if (typeflag == FTW_F) {
        if (unlink(fpath) == -1) {
            perror("Failed to delete a file inside the file temp dir");
            return -1;
        }
    }

    return 0;
}

int createBalcao(void) {

    pthread_mutex_lock(&sharedMemory->nBalcoesMutex);
    while (sharedMemory->nBalcoes >= MAXBALCOES)
        pthread_cond_wait(&sharedMemory->nBalcoesCondvar, &sharedMemory->nBalcoesMutex);

    Info_t *ptrBalcao = &sharedMemory->infoBalcoes[sharedMemory->nBalcoes];
    ptrBalcao->tempoAbertura = tempoAbertura;
    ptrBalcao->sumatorioTempoAtendimentoClientes = 0;
    ptrBalcao->nClientesAtendidos = 0;
    ptrBalcao->fifoCircularIndex = 0;
    ptrBalcao->fifoSlots = MAXCLIENTES;

    // Initialize the mutexes and condvars
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&ptrBalcao->namedPipeMutex, &mattr);
    pthread_mutex_init(&ptrBalcao->clientContribMutex, &mattr);
    pthread_mutex_init(&ptrBalcao->fifoMutex, &mattr);
    pthread_mutex_init(&ptrBalcao->fifoSlotsMutex, &mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr,PTHREAD_PROCESS_SHARED);

    pthread_cond_init(&ptrBalcao->namedPipeCondvar, &cattr);
    pthread_cond_init(&ptrBalcao->clientContribCondvar, &cattr);
    pthread_cond_init(&ptrBalcao->fifoCondvar, &cattr);
    pthread_cond_init(&ptrBalcao->fifoSlotsCondvar, &cattr);

    char fifoName[4];
    snprintf(fifoName, 4, "%s%lu", "b", sharedMemory->nBalcoes);
    if (mkfifo("b", 0660) != 0) {
        return -1;
    }

    int fifoFd = open(fifoName, O_RDONLY);
    if (fifoFd == -1) {
        return -1;
    }
    ptrBalcao->namedPipeFd = fifoFd;

    ++sharedMemory->nBalcoes;

    pthread_mutex_unlock(&sharedMemory->nBalcoesMutex);
    pthread_cond_broadcast(&sharedMemory->nBalcoesCondvar);

    return 0;
}

