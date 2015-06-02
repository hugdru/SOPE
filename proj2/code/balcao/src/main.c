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
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>

#define MAXBALCOES 32
#define MAXCLIENTES 128
#define NAMEDPIPESIZE (2 + 2 + 1 + 7 + 1)
#define MAXTHREADS 128

/** Estruturas que serão partilhadas **/
typedef struct Info {
    /** namedPipe and Misc**/
    char namedPipeName[NAMEDPIPESIZE];
    pthread_mutex_t namedPipeMutex;
    pthread_cond_t namedPipeCondvar;
    /** End of namedPipe **/
    /** Client Contributions **/
    size_t sumatorioTempoAtendimentoClientes;
    size_t nClientesAtendidos;
    pthread_mutex_t clientContribMutex;
    pthread_cond_t clientContribCondvar;
    /** End of Client Contributions **/
    /** Client Look **/
    size_t nClientesEmAtendimento;
    int aberto;
    pthread_mutex_t clientLookMutex;
    pthread_cond_t clientLookCondvar;
    /** End of Client Look **/
    time_t tempoInicioFuncionamento;
} Info_t;

typedef struct SharedMemory {
    Info_t infoBalcoes[MAXBALCOES];
    size_t nBalcoes;
    pthread_mutex_t nBalcoesMutex;
    pthread_cond_t nBalcoesCondvar;
    time_t tempoAbertura;
} SharedMemory_t;
/** Fim de Estrutas que serão partilhadas **/

/** Stuff to put in structure or that can be access globally **/
int namedPipeFd = -1;
char *semName = NULL;
char *shmName = NULL;
size_t numeroBalcao;
long unsigned int tempoAbertura;
SharedMemory_t *sharedMemory = NULL;
sem_t *globalShemaphore = NULL;
/** Fim de Stuff to put in structure or that can be access globally **/

const char tempFilePathDir[] = "/tmp/sope";

sem_t* getGlobalSemaphore(void);
int destroyGlobalSemaphore(void);
SharedMemory_t* createSharedMemory(void);
int destroySharedMemory(void);
SharedMemory_t* openSharedMemory(void);
int createFolder(void);
int removeTempDir(void);
int ftwHandler(const char *fpath,__attribute__((unused)) const struct stat *sb, int typeflag);
int createBalcao(void);
void alarmHandler(__attribute__((unused)) int signo);
void *answerCall(void *arg);
int generateStatistics(void);

pthread_mutex_t answerThreadWorkingMutex;
pthread_cond_t answerThreadWorkingCondvar;

size_t nThreads = 0;
int bailOutOnNextClient = 0;
pthread_mutex_t nThreadsMutex;
pthread_cond_t nThreadsCondvar;

int main(int argc, char *argv[]) {

    int failed = 0;

    if (argc != 3) {
        fprintf(stderr, "balcao <nome_mempartilhada> <tempo_abertura>\n");
        exit(EXIT_FAILURE);
    }

    // Add the / prefix to the parameter received
    size_t shmNameSize = (strlen(argv[1]) + 1) * sizeof(argv[1][0]) + 1;
    shmName = (char *) malloc(shmNameSize);
    if (shmName == NULL) {
        perror("Failure in malloc");
        exit(EXIT_FAILURE);
    }
    snprintf(shmName, shmNameSize, "%s%s", "/", argv[1]);

    // Convert from string to unsigned long
    errno = 0;
    tempoAbertura = strtoul(argv[2], NULL, 10);
    if (errno != 0) {
        perror("Failure in strtoul()");
        free(shmName);
        exit(EXIT_FAILURE);
    }

    // Open or create the semaphore initialize with 1
    globalShemaphore = getGlobalSemaphore();
    if (globalShemaphore == NULL) {
        fprintf(stderr, "Failure in globalShemaphore()\n");
        free(shmName);
    }
    printf("Caught globalShemaphore\n");

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
        printf("Created the shared Memory\n");

        if (createFolder() != 0) {
            perror("Failure in createFolder");
            goto cleanUp;
        }

    } else {
        printf("Openned the shared Memory\n");
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
        perror("Failure in createBalcao()");
        goto cleanUp;
    }
    printf("Created Balcao %lu\n", numeroBalcao);

    // Install the alarm stuff
    struct sigaction sigact;
    sigact.sa_handler = alarmHandler;
    sigemptyset(&sigact.sa_mask);
    // Do not receive job control notification from child
    if (sigaction(SIGALRM, &sigact, NULL) == -1) {
        perror("There was an error installing the SIGALRM handler");
        goto cleanUp;
    }

    // Wait for clents and act accordingly
    char intel[256];
    int intelFilledSize = 0;
    ssize_t intelReadSize;
    int ptrStartTokenIndex = 0;
    int ptrLastSeparatorIndex = 0;
    Info_t *thisBalcao = &sharedMemory->infoBalcoes[numeroBalcao];

    // Start alarm
    alarm((unsigned int) (tempoAbertura));

    while (1) {
        if (pthread_mutex_lock(&thisBalcao->namedPipeMutex) != 0) {
            fprintf(stderr, "Failure in pthread_mutex_lock()\n");
            goto cleanUp;
        }

        if (pthread_cond_wait(&thisBalcao->namedPipeCondvar, &thisBalcao->namedPipeMutex) != 0) {
            fprintf(stderr, "Failure in pthread_cond_wait()\n");
            goto cleanUp;
        }
        if (bailOutOnNextClient) {
            if (pthread_mutex_unlock(&thisBalcao->namedPipeMutex) != 0) {
                fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
                goto cleanUp;
            }
            break;
        }

        if ((intelReadSize = read(namedPipeFd, intel + intelFilledSize, (size_t) (256 - intelFilledSize))) == -1) {
            perror("Failure in read");
            goto cleanUp;
        }

        if (pthread_mutex_unlock(&thisBalcao->namedPipeMutex) != 0) {
            fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
            goto cleanUp;
        }

        pthread_t answerThreadId;

        if (intelReadSize != 0) {
            int i = intelFilledSize;
            ptrStartTokenIndex = 0;
            while (i < intelReadSize) {
                if (intel[i + intelFilledSize] == '\0') {

                    ptrLastSeparatorIndex = i;

                    size_t clientNamedPipeSize = (size_t) (ptrLastSeparatorIndex - ptrStartTokenIndex + 1);
                    char *clientNamedPipe = (char *) malloc(clientNamedPipeSize);
                    if (clientNamedPipe == NULL) {
                        perror("Failure in malloc()");
                        goto cleanUp;
                    }
                    snprintf(clientNamedPipe, clientNamedPipeSize, "%s", &intel[ptrStartTokenIndex]);

                    printf("Balcao %lu, Attempting to answer to client, clientNamedPipe %s\n", numeroBalcao, clientNamedPipe);
                    pthread_create(&answerThreadId, NULL, answerCall, clientNamedPipe);

                    ptrStartTokenIndex = i + 1;
                }
                ++i;
            }

            if (ptrStartTokenIndex != intelReadSize) {
                intelFilledSize = 255 - ptrLastSeparatorIndex;
                if (memcpy(intel, intel + ptrStartTokenIndex, (size_t) intelFilledSize) == NULL) {
                    perror("Failure in memcpy");
                    goto cleanUp;
                }
            }
        }

        if (bailOutOnNextClient) break;
    }

    // Disable this balcao namedPipe
    if (namedPipeFd != -1) {
        if (close(namedPipeFd) == -1) {
            perror("Failure in close()");
        }

        if (unlink(thisBalcao->namedPipeName) == -1) {
            perror("Failure in unlink()");
        }
    }

    // Wait for the current client and store the ones left to answer
    printf("Balcao %lu Time expired, waiting for the last working one and the rest to update the remaining queue size\n", numeroBalcao);
    pthread_mutex_lock(&nThreadsMutex);
    while (nThreads != 0) {
        pthread_cond_wait(&nThreadsCondvar, &nThreadsMutex);
    }
    pthread_mutex_unlock(&nThreadsMutex);

    // Close this balcao
    printf("Closing a balcao %lu\n", numeroBalcao);
    pthread_mutex_lock(&thisBalcao->clientLookMutex);
    thisBalcao->aberto = 0;
    pthread_mutex_unlock(&thisBalcao->clientLookMutex);

    // Check to see if we are the last balcao
    pthread_mutex_lock(&sharedMemory->nBalcoesMutex);
    size_t nBalcoes = sharedMemory->nBalcoes;

    int cleanAndGenerateStatistics = 1;
    for (size_t i = 0; i < nBalcoes; ++i) {
        pthread_mutex_lock(&sharedMemory->infoBalcoes[i].clientLookMutex);
        if (sharedMemory->infoBalcoes[i].aberto) {
            cleanAndGenerateStatistics = 0;
            break;
        }
        pthread_mutex_unlock(&sharedMemory->infoBalcoes[i].clientLookMutex);
    }

    if (!cleanAndGenerateStatistics) {
        pthread_mutex_unlock(&sharedMemory->nBalcoesMutex);
        EXIT_SUCCESS;
    }

    printf("Generating statistics\n");
    if (generateStatistics() != 0) {
        pthread_mutex_unlock(&sharedMemory->nBalcoesMutex);
        fprintf(stderr, "Failure in generateStatistics\n");
        EXIT_FAILURE;
    }

    pthread_mutex_unlock(&sharedMemory->nBalcoesMutex);

cleanUp:

    printf("Balcao %lu\n", numeroBalcao);
    printf("Destroying shared memory\n");
    if (destroySharedMemory() != 0) {
        fprintf(stderr, "Failure in destroySharedMemory()\n");
        failed = 1;
    }

    printf("Destroying semaphore\n");
    if (destroyGlobalSemaphore() != 0) {
        fprintf(stderr, "Failure in destroyGlobalSemaphore()\n");
        failed = 1;
    }

    printf("Removing the temp directory\n");
    if (removeTempDir() != 0) {
        fprintf(stderr, "Failure in removeTempDir()\n");
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
    shm->tempoAbertura = time(NULL);

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

int destroySharedMemory(void) {

    if (sharedMemory == NULL || shmName == NULL) {
        errno = EINVAL;
        perror("Failure in destroySharedMemory");
        return -1;
    }

    if (munmap(sharedMemory, sizeof(SharedMemory_t)) < 0) {
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

int destroyGlobalSemaphore(void) {
    if (globalShemaphore == NULL) {
        errno = EINVAL;
        perror("Failure in createGlobalSemaphore");
        return -1;
    }

    int failed = 0;
    if (sem_close(globalShemaphore) != 0) {
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

    if (pthread_mutex_lock(&sharedMemory->nBalcoesMutex) != 0) {
        fprintf(stderr, "Failure in pthread_mutex_lock()\n");
        return -1;
    }

    while ((numeroBalcao = sharedMemory->nBalcoes) >= MAXBALCOES) {
        if (pthread_cond_wait(&sharedMemory->nBalcoesCondvar, &sharedMemory->nBalcoesMutex) != 0) {
            fprintf(stderr, "Failure in pthread_cond_wait()\n");
            return -1;
        }
    }

    Info_t *ptrBalcao = &sharedMemory->infoBalcoes[numeroBalcao];
    ptrBalcao->sumatorioTempoAtendimentoClientes = 0;
    ptrBalcao->nClientesAtendidos = 0;
    ptrBalcao->nClientesEmAtendimento = 0;
    ptrBalcao->aberto = 1;
    ptrBalcao->tempoInicioFuncionamento = time(NULL);

    // Initialize the mutexes and condvars
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&ptrBalcao->namedPipeMutex, &mattr);
    pthread_mutex_init(&ptrBalcao->clientContribMutex, &mattr);
    pthread_mutex_init(&ptrBalcao->clientLookMutex, &mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);

    pthread_cond_init(&ptrBalcao->namedPipeCondvar, &cattr);
    pthread_cond_init(&ptrBalcao->clientContribCondvar, &cattr);
    pthread_cond_init(&ptrBalcao->clientLookCondvar, &cattr);

    snprintf(ptrBalcao->namedPipeName, NAMEDPIPESIZE, "fb%lu_%d", numeroBalcao, getpid());
    if (mkfifo(ptrBalcao->namedPipeName, 0660) != 0) {
        perror("Failure in mkfifo()");
        return -1;
    }

    int fifoFd = open(ptrBalcao->namedPipeName, O_RDONLY | O_NONBLOCK);
    if (fifoFd == -1) {
        perror("Failure in open()");
        return -1;
    }
    namedPipeFd = fifoFd;

    ++sharedMemory->nBalcoes;

    if (pthread_cond_broadcast(&sharedMemory->nBalcoesCondvar) != 0) {
        fprintf(stderr, "Failure in pthread_cond_broadcast()\n");
        return -1;
    }

    if (pthread_mutex_unlock(&sharedMemory->nBalcoesMutex) != 0) {
        fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
        return -1;
    }

    return 0;
}

void alarmHandler(__attribute__((unused)) int signo) {

    pthread_mutex_lock(&nThreadsMutex);
    bailOutOnNextClient = 1;
    pthread_mutex_unlock(&nThreadsMutex);

    Info_t *thisBalcao = &sharedMemory->infoBalcoes[numeroBalcao];
    pthread_cond_broadcast(&thisBalcao->namedPipeCondvar);
}

void *answerCall(void *arg) {

    if (arg == NULL) {
        errno = EINVAL;
        perror("answerCall wrong arguments");
    }

    Info_t *balcao = &sharedMemory->infoBalcoes[numeroBalcao];
    pthread_mutex_lock(&balcao->clientLookMutex);
    ++balcao->nClientesEmAtendimento;
    size_t nClientesEmAtendimento = balcao->nClientesEmAtendimento;
    pthread_mutex_unlock(&balcao->clientLookMutex);

    pthread_mutex_lock(&nThreadsMutex);
    ++nThreads;
    if (bailOutOnNextClient) {
        free(arg);
        --nThreads;
        return NULL;
    }
    pthread_mutex_unlock(&nThreadsMutex);

    pthread_mutex_lock(&answerThreadWorkingMutex);

    char *clientNamedPipe = (char *) arg;

    int clientNamedPipeFd = open(clientNamedPipe, O_WRONLY);
    if (clientNamedPipeFd == -1) {
        perror("Failure in open()\n");
    }

    size_t sleepSeconds;
    if (nClientesEmAtendimento > 10) {
        sleepSeconds = 10;
    } else {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        srand((unsigned int) (tv.tv_sec * tv.tv_usec));
        sleepSeconds = (unsigned int) (rand() % 10 + 1);
    }
    // Sleep x time
    sleep((unsigned int) (sleepSeconds));

    pthread_mutex_lock(&balcao->clientLookMutex);
    --balcao->nClientesEmAtendimento;
    pthread_mutex_unlock(&balcao->clientLookMutex);

    pthread_mutex_lock(&balcao->clientContribMutex);
    ++balcao->nClientesAtendidos;
    balcao->sumatorioTempoAtendimentoClientes += sleepSeconds;
    pthread_mutex_unlock(&balcao->clientContribMutex);

    if (write(clientNamedPipeFd, "fim_atendimento", sizeof("fim_atendimento")) == -1) {
        perror("Failure in read()");
    }

    pthread_mutex_unlock(&answerThreadWorkingMutex);

    pthread_mutex_lock(&nThreadsMutex);
    --nThreads;
    pthread_cond_signal(&nThreadsCondvar);
    pthread_mutex_unlock(&nThreadsMutex);

    free(clientNamedPipe);
    return NULL;
}

int generateStatistics(void) {

}
