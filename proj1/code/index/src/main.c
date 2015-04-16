#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "Files.h"

#define DEFAULTBUFFERSIZE 128
#define DIRPATHINDEX 1
#define PIPEREAD 0
#define PIPEWRITE 1

long nChildProcesses = 0;

const char defaultWordsFileName[] = "words.txt";

#define TEMPFILENUMBERSLEN 20
const char tempFilesDir[] = "/tmp/indexTemp/";
const size_t defaultTempFilesPathSize = (sizeof(tempFilesDir) / sizeof(tempFilesDir[0])) + TEMPFILENUMBERSLEN;
unsigned long int tempFileName = 0;

void childHandler(int signo);

int main(int argc, char *argv[]) {

    const long maxChildProcesses = sysconf(_SC_CHILD_MAX) / 4;

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get the files to be worked on
    Files_t* files = getAllFilesNames(argv[DIRPATHINDEX], defaultWordsFileName);
    if (files == NULL) {
        perror("Something went wrong finding the files");
        exit(EXIT_FAILURE);
    }

    int escape = false;
    if (files->numberOfFiles == 0) {
        fprintf(stderr, "You must create files to be indexed\n");
        escape = true;
    }
    if (!files->foundDefaultWordsFileName) {
        fprintf(stderr, "You must create the %s file\n", defaultWordsFileName);
        escape = true;
    }
    if (escape) exit(EXIT_FAILURE);

    /** INSTALL SIGCHLD HANDLER AND FILL SUSPEND MASK**/
    // Because we don't use them and with don't want zombies
    struct sigaction sigact;
    sigact.sa_handler = childHandler;
    // Do not receive job control notification from child
    sigact.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
        perror("There was an error installing the SIGCHLD handler");
        exit(EXIT_FAILURE);
    }

    sigset_t sigmask;
    if (sigfillset(&sigmask) == -1 || sigdelset(&sigmask, SIGCHLD) == -1) {
        perror("Failed setting the mask for SIGCHLD suspend if nChildProcesses == maxChildProcesses");
        exit(EXIT_FAILURE);
    }
    /** END OF INSTALL SIGCHLD HANDLER AND FILL SUSPEND MASK**/

    for (size_t index = 0; index < files->numberOfFiles; ++index) {

        if (nChildProcesses == maxChildProcesses) sigsuspend(&sigmask);
        ++nChildProcesses;

        pid_t pidSw = fork();

        switch (pidSw) {
            case -1:
                {
                    perror("Failed to create a sw child process");
                    exit(EXIT_FAILURE);
                    break;
                }
            case 0:
                {
                    // Append number to string
                    /*int newFileDescriptor;*/

                    /*char **newFilePathBuffer = NULL;*/
                    /*int newFilePathSize =*/
                    /*if (mycatsi(&newFilePathBuffer, &newFilePathSize, ptr1, tempFileName) == -1) {*/
                    /*perror("Failed to append int to string");*/
                    /*exit(EXIT_FAILURE);*/
                    /*}*/
                    /*if ((newFileDescriptor = open(*/
                    /*if (dup2(newFileDescriptor, STDOUT_FILENO) == -1) {*/
                    /*perror("Failed to redirect stdout in child");*/
                    /*exit(EXIT_FAILURE);*/
                    /*}*/

                    execl("../sw", "sw", defaultWordsFileName, files->filesNamesToSearch[index], NULL);
                    fprintf(stderr, "failed to exec sw\n");
                    exit(EXIT_FAILURE);
                    break;
                }
            default:
                /*++tempFileName;*/
                break;
        }
    }

    // Dynamically Allocated Array
    wipe(files);

    return EXIT_SUCCESS;
}

void childHandler(__attribute__((unused)) int signo) {
    for (int i = 0; i < nChildProcesses; ++i) {
        int status;
        if (waitpid(-1, &status, WNOHANG) == -1) {
            perror("something wrong with waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            nChildProcesses--;
        } else if (WIFSIGNALED(status)) {
            perror("child killed by signal");
            exit(EXIT_FAILURE);
        }
    }
}

