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
#include <sys/wait.h>

#define MAXBALCOES 32
#define MAXCLIENTES 128
#define NAMEDPIPESIZE (2 + 2 + 1 + 7 + 1)


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

SharedMemory_t* openSharedMemory(char *shmName);
void childHandler(__attribute__((unused)) int signo);
void pipeHandler(__attribute__((unused)) int signo);

const char tempFilePathDir[] = "/tmp/sope";

size_t nChildProcesses = 0;

int main(int argc, char *argv[]) {

    int failed = 0;

    if (argc != 3) {
        fprintf(stderr, "ger_cl <nome_mempartilhada> <num_clientes>");
        exit(EXIT_FAILURE);
    }

    size_t shmNameSize = (strlen(argv[1]) + 1) * sizeof(argv[1][0]) + 1;
    char *shmName = (char *) malloc(shmNameSize);
    if (shmName == NULL) {
        perror("Failure in malloc()");
        exit(EXIT_FAILURE);
    }
    snprintf(shmName, shmNameSize, "%s%s", "/", argv[1]);

    // Convert from string to unsigned long
    errno = 0;
    size_t numClientes = strtoul(argv[2], NULL, 10);
    if (errno != 0) {
        perror("Failure in strtoul()");
        goto cleanUp;
    }

    SharedMemory_t *sharedMemory;
    if ((sharedMemory = openSharedMemory(shmName)) == NULL) {
        fprintf(stderr, "Failure in openSharedMemory\n");
        goto cleanUp;
    }
    free(shmName);
    shmName = NULL;

    if (chdir(tempFilePathDir) == -1) {
        perror("Failure in chdir()");
        goto cleanUp;
    }

    /** INSTALL SIGCHLD HANDLER **/
    struct sigaction sigactChild;
    sigactChild.sa_handler = childHandler;
    sigemptyset(&sigactChild.sa_mask);
    // Do not receive job control notification from child
    sigactChild.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sigactChild, NULL) == -1) {
        perror("There was an error installing the SIGCHLD handler");
        goto cleanUp;
    }

    for (size_t i = 0; i < numClientes; ++i) {

        ++nChildProcesses;
        pid_t newClientePid = fork();
        if (newClientePid == -1) {
            perror("Failure in fork");
            goto cleanUp;
        }

        switch (newClientePid) {
            case 0:
                {
                    /** INSTALL SIGPIPE HANDLER **/
                    struct sigaction sigactPipe;
                    sigactPipe.sa_handler = pipeHandler;
                    sigemptyset(&sigactPipe.sa_mask);
                    if (sigaction(SIGPIPE, &sigactPipe, NULL) == -1) {
                        perror("There was an error installing the SIGPIPE handler");
                        exit(EXIT_FAILURE);
                    }

                    size_t clientNamedPipeSize = 3 + 7 + 1;
                    char *clientNamedPipeName = (char *) malloc(clientNamedPipeSize);
                    if (clientNamedPipeName == NULL) {
                        perror("Failure in namedPipeName()");
                        exit(EXIT_FAILURE);
                    }

                    snprintf(clientNamedPipeName, clientNamedPipeSize, "fc_%d", getpid());
                    if (mkfifo(clientNamedPipeName, 0660) != 0) {
                        perror("Failure in mkfifo()");
                        goto cleanUpChild;
                    }

                    if (pthread_mutex_lock(&sharedMemory->nBalcoesMutex) != 0) {
                        fprintf(stderr, "Failure in pthread_mutex_lock()\n");
                        goto cleanUpChild;
                    }
                    size_t nBalcoes = sharedMemory->nBalcoes;
                    if (pthread_mutex_unlock(&sharedMemory->nBalcoesMutex) != 0) {
                        fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
                        goto cleanUpChild;
                    }

                    size_t indexMinSlots = 0;
                    size_t minSlots = 0;
                    int foundAtLeastOne = 0;

                    for (size_t t = 0; t < nBalcoes; ++t) {

                        if (pthread_mutex_lock(&sharedMemory->infoBalcoes[t].clientLookMutex) != 0) {
                            fprintf(stderr, "Failure in pthread_mutex_lock()\n");
                            goto cleanUpChild;
                        }

                        size_t nEmAtendimento = sharedMemory->infoBalcoes[t].nClientesEmAtendimento;
                        int aberto = sharedMemory->infoBalcoes[t].aberto;

                        if (!aberto) {
                            if (pthread_mutex_unlock(&sharedMemory->infoBalcoes[t].clientLookMutex) != 0) {
                                fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
                                goto cleanUpChild;
                            }
                            continue;
                        }
                        foundAtLeastOne = 1;

                        if (pthread_mutex_unlock(&sharedMemory->infoBalcoes[t].clientLookMutex) != 0) {
                            fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
                            goto cleanUpChild;
                        }

                        if (t == 0) {
                            minSlots = nEmAtendimento;
                            continue;
                        }
                        if (minSlots > nEmAtendimento) {
                            minSlots = nEmAtendimento;
                            indexMinSlots = t;
                        }
                    }

                    if (!foundAtLeastOne) {
                        fprintf(stderr, "Every balcao is closed");
                        goto cleanUpChild;
                    }

                    Info_t *chosenBalcao = &sharedMemory->infoBalcoes[indexMinSlots];
                    if (pthread_mutex_lock(&chosenBalcao->namedPipeMutex) != 0) {
                        fprintf(stderr, "Failure in pthread_mutex_lock\n");
                        goto cleanUpChild;
                    }

                    int fifoFd = open(chosenBalcao->namedPipeName, O_WRONLY);
                    if (fifoFd == -1) {
                        perror("Failure in open()");
                        goto cleanUpChild;
                    }

                    if (write(fifoFd, clientNamedPipeName, strlen(clientNamedPipeName) + 1) == -1) {
                        perror("Failure in write()");
                        goto cleanUpChild;
                    }

                    if (pthread_cond_broadcast(&chosenBalcao->namedPipeCondvar) != 0) {
                        fprintf(stderr, "Failure in pthread_cond_signal()\n");
                        goto cleanUpChild;
                    }

                    if (pthread_mutex_unlock(&chosenBalcao->namedPipeMutex) != 0) {
                        fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
                        goto cleanUpChild;
                    }

                    // Wait for the return message so that we can close our clientNamedPipe
                    // The open is blocked until something is opened for writing

                    if (pthread_mutex_lock(&chosenBalcao->clientLookMutex) != 0) {
                        fprintf(stderr, "Failure in pthread_mutex_lock()\n");
                        goto cleanUpChild;
                    }

                    int aberto = chosenBalcao->aberto;

                    if (pthread_mutex_unlock(&chosenBalcao->clientLookMutex) != 0) {
                        fprintf(stderr, "Failure in pthread_mutex_unlock()\n");
                        goto cleanUpChild;
                    }

                    if (aberto) {
                        int clientFifoFd = open(clientNamedPipeName, O_RDONLY);
                        if (clientFifoFd == -1) {
                            perror("Failure in open()\n");
                            goto cleanUpChild;
                        }

                        char mensagem[64];
                        ssize_t bytesRead;
                        if ((bytesRead = read(clientFifoFd, mensagem, 64)) == -1) {
                            perror("Failure in read()");
                            goto cleanUpChild;
                        }
                        mensagem[bytesRead - 1] = '\0';
                        puts(mensagem);

                        if (close(clientFifoFd) == -1) {
                            perror("Failure in close()");
                        }

                        if (unlink(clientNamedPipeName) == -1) {
                            perror("Failure in unlink()");
                        }
                    } else {
                        fprintf(stderr, "Balcao is closed\n");
                    }

                    free(shmName);
                    exit(EXIT_SUCCESS);
cleanUpChild:
                    if (clientNamedPipeName != NULL) free(clientNamedPipeName);
                    if (shmName != NULL) free(shmName);
                    exit(EXIT_FAILURE);
                }
        }
    }

    // Uninstall the child handler
    sigemptyset(&sigactChild.sa_mask);
    sigactChild.sa_handler = SIG_DFL;
    sigactChild.sa_flags = 0;
    if (sigaction(SIGCHLD, &sigactChild, NULL) == -1) {
        perror("There was an error setting the default SIGCHLD handler");
        goto cleanUp;
    }

    // Waits for all the children to end
    while (nChildProcesses > 0) {
        int status;
        if (wait(&status) == -1) {
            perror("There was an error waiting for all the sws to finish");
            goto cleanUp;
        }
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "Something wrong with a child\n");
                goto cleanUp;
            }
            --nChildProcesses;
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child killed by signal\n");
            goto cleanUp;
        }
    }

cleanUp:
    if (shmName != NULL) free(shmName);

    if (failed) exit(EXIT_FAILURE);
    else exit(EXIT_SUCCESS);

}

SharedMemory_t* openSharedMemory(char *shmName) {
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


void childHandler(__attribute__((unused)) int signo) {
    for (size_t i = 0; i < nChildProcesses; ++i) {
        int status;
        if (waitpid(-1, &status, WNOHANG) == -1) {
            perror("something wrong with waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "something wrong with a child\n");
                exit(EXIT_FAILURE);
            }
            nChildProcesses--;
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child killed by signal\n");
            exit(EXIT_FAILURE);
        }
    }
}

void pipeHandler(__attribute__((unused)) int signo) {
    fprintf(stderr, "Balcao was closed\n");
    exit(EXIT_FAILURE);
}
