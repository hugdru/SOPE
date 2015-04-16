#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <ftw.h>
#include <errno.h>

#include "Files.h"

#define DIRPATHINDEX 1
#define PIPEREAD 0
#define PIPEWRITE 1

long nChildProcesses = 0;

const char defaultWordsFileName[] = "words.txt";

const char tempFilePathDir[] = "/tmp/index";
// "/tmp/index-pid/fileName"
const size_t tempFilePathDirBufSize = (sizeof(tempFilePathDir) / sizeof(tempFilePathDir[0])) + (1 + 7 + 1) * sizeof(char);
const size_t tempFilePathBufSize = (sizeof(tempFilePathDir) / sizeof(tempFilePathDir[0])) + (1 + 7 + 1 + NAME_MAX) * sizeof(char);

void childHandler(int signo);
void removeTempDir(char *dirPath);
int ftwHandler(const char *fpath, const struct stat *sb, int typeflag);

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
    sigemptyset(&sigact.sa_mask);
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
    char *tempFilePath = (char *) malloc(tempFilePathDirBufSize * sizeof(char));
    int tempFilePathLen = snprintf(tempFilePath, tempFilePathDirBufSize, "%s-%d/", tempFilePathDir, getpid());
    if (tempFilePathLen < 0) {
        perror("Failed to create string temp file path dir");
        exit(EXIT_FAILURE);
    }

    if (mkdir(tempFilePath, 0770) == -1) {
        perror("Failed to create temp file folder");
        if (errno == EEXIST) {
            fprintf(stderr, "Remove the %s directory manually\n", tempFilePath);
        }
        exit(EXIT_FAILURE);
    }

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
                    tempFilePath = (char *) realloc(tempFilePath, tempFilePathBufSize);
                    if (tempFilePath == NULL) {
                        perror("Failed to allocate space for a temp file path");
                        exit(EXIT_FAILURE);
                    }
                    snprintf(
                            &tempFilePath[tempFilePathLen], NAME_MAX,
                            "%s",
                            files->filesNamesToSearch[index]
                            );


                    int newTempFileDescriptor;
                    if ((newTempFileDescriptor = open(tempFilePath, O_WRONLY | O_CREAT | O_EXCL, 0700)) == -1) {
                        perror("There was an error creating a temporary file");
                        exit(EXIT_FAILURE);
                    }
                    if (dup2(newTempFileDescriptor, STDOUT_FILENO) == -1) {
                        perror("Failed to redirect child stdout");
                        exit(EXIT_FAILURE);
                    }

                    execl("../sw", "sw", defaultWordsFileName, files->filesNamesToSearch[index], NULL);
                    free(tempFilePath);
                    fprintf(stderr, "failed to exec sw\n");
                    exit(EXIT_FAILURE);
                    break;
                }
            default:
                break;
        }
    }
    // Wait for all children to finish
    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = SIG_DFL;
    sigact.sa_flags = 0;
    if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
        perror("There was an error setting the default SIGCHLD handler");
        exit(EXIT_FAILURE);
    }

    do {
        if (wait(NULL) == -1) {
            if (errno == ECHILD) break;
        }
    } while (true);

    // Cleansing
    wipe(files);
    removeTempDir(tempFilePath);
    free(tempFilePath);


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

void removeTempDir(char *dirPath) {
    if (dirPath == NULL) {
        return;
    }
    if (ftw(dirPath, ftwHandler, 5) == -1) {
        perror("Failed to clean files inside file temp dir");
        exit(EXIT_FAILURE);
    }

    if (rmdir(dirPath) == -1) {
        perror("Can't delete the file temp dir");
        exit(EXIT_FAILURE);
    }
}

int ftwHandler(const char *fpath,__attribute__((unused)) const struct stat *sb, int typeflag) {
    if (typeflag == FTW_F) {
        if (unlink(fpath) == -1) {
            perror("Failed to delete a file inside the file temp dir");
            exit(EXIT_FAILURE);
        }
    }

    return 0;
}

