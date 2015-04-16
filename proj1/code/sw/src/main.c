#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#define wordsFilePathIndex 1
#define searchFilePathIndex 2
#define PIPEREAD 0
#define PIPEWRITE 1

#define CHAR_BUFFER_SIZE 128
#define MAX_CHILD_PROCESSES = 200;

long nChildProcesses = 0;
long maxChildProcesses;

const char sedSubBeginning[] = "s/([0-9]*).*/";
const char sedSubEnd[] = "\\1/";
const char firstSeparatorString[] = " : ";
const char secondSeparatorString[] = "-";
const size_t sedConstantLen =
    (sizeof(sedSubBeginning) + sizeof(sedSubEnd) +
     sizeof(firstSeparatorString) + sizeof(secondSeparatorString) - 4) / sizeof(sedSubBeginning[0]);

void childHandler(int signo);
int findIndexLastChar(char *ptr, char ch, int *len);

int main(int argc, char *argv[]) {

    maxChildProcesses = sysconf(_SC_CHILD_MAX) / 4;

    if (argc != 3) {
        fprintf(stderr, "%s wordsFileName searchFileName\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *wordsStream;
    if ((wordsStream = fopen(argv[wordsFilePathIndex], "r")) == NULL) {
        perror("Can't find the file with the words");
        return EXIT_FAILURE;
    }

    struct stat stat_buf;
    int result = stat(argv[searchFilePathIndex], &stat_buf);
    if (result != 0 || !S_ISREG(stat_buf.st_mode)) {
        errno = EINVAL;
        perror("bad search file name");
        return EXIT_FAILURE;
    }

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

    // fileName no extension no slash
    int searchFileNameLen = 0;
    int lastSlashIndex = findIndexLastChar(argv[searchFilePathIndex], '/', &searchFileNameLen);
    int lastDotIndex = findIndexLastChar(&argv[searchFilePathIndex][lastSlashIndex + 1], '.', NULL);
    char *searchFileName = argv[searchFilePathIndex];

    if (lastSlashIndex != -1 || lastDotIndex != -1) {
        if (lastSlashIndex != -1 && lastDotIndex != -1) {
            lastDotIndex += lastSlashIndex + 1;
        } else if (lastDotIndex == -1) {
            lastDotIndex = searchFileNameLen + 1;
        }
        printf("lastSlashIndex %d lastDotIndex %d", lastSlashIndex , lastDotIndex);
        searchFileNameLen = lastDotIndex - lastSlashIndex;
        printf(" size %d", searchFileNameLen);
        if (searchFileNameLen < 0) {
            exit(EXIT_FAILURE);
        }
        searchFileName = (char *) malloc(sizeof(char) * (size_t)(searchFileNameLen + 1));
        snprintf(searchFileName, (size_t)searchFileNameLen, "%s", &argv[searchFilePathIndex][lastSlashIndex + 1]);
    }

    // Buffer for getline, if it is not enough it reallocs
    size_t lineBufferSize = CHAR_BUFFER_SIZE;
    char *linePtr = (char *) malloc(sizeof(char) * CHAR_BUFFER_SIZE);

    // Buffer for sed substitute parameter
    size_t sedBufferSize = CHAR_BUFFER_SIZE;
    char *sedBufferPtr = (char *) malloc(sizeof(char) * CHAR_BUFFER_SIZE);

    // sw    grep ->[]- sed ->[]--(stdout)
    ssize_t bytesRead;
    while ((bytesRead = getline(&linePtr, &lineBufferSize, wordsStream)) > 0) {

        if (linePtr[bytesRead - 1] == '\n') linePtr[bytesRead - 1] = '\0';

        /** GREP SECTION **/
        int pipeGrepSed[2];
        if (pipe(pipeGrepSed) < 0) {
            perror("Could not create a pipe cut -> sw");
            exit(EXIT_FAILURE);
        }

        if (nChildProcesses == maxChildProcesses) sigsuspend(&sigmask);
        ++nChildProcesses;
        pid_t pidForkGrep = fork();
        if (pidForkGrep < 0) {
            perror("Failed to create a grep child process");
            exit(EXIT_FAILURE);
        }

        switch (pidForkGrep) {
            case -1:
                perror("Failed to fork cut");
                exit(EXIT_FAILURE);
                break;
            case 0:
                close(pipeGrepSed[PIPEREAD]);
                dup2(pipeGrepSed[PIPEWRITE], STDOUT_FILENO);
                execlp("grep", "grep", linePtr, "-i", "-n", argv[searchFilePathIndex], NULL);
                fprintf(stderr, "failed to exec grep\n");
                exit(EXIT_FAILURE);
                break;
            default:
                // I can't close the pipeGrepCut[PIPEREAD] because the OS will see that there are
                // no processes conected to the read end and will generate a SIGPIPE signal
                close(pipeGrepSed[PIPEWRITE]);
                break;
        }
        /** END OF GREP SECTION **/
        /** START OF SED SECTION **/

        if (nChildProcesses == maxChildProcesses) sigsuspend(&sigmask);
        ++nChildProcesses;
        pid_t pidForkSed = fork();
        if (pidForkSed < 0) {
            perror("Failed to create a cut child process");
            exit(EXIT_FAILURE);
        }

        switch (pidForkSed) {
            case -1:
                perror("Failed to fork grep");
                exit(EXIT_FAILURE);
                break;
            case 0:
                dup2(pipeGrepSed[PIPEREAD], STDIN_FILENO);
                // Do the sed magic
                size_t sedStringArgSize = sedConstantLen + (size_t)searchFileNameLen + strlen(linePtr) + 1;
                if (sedStringArgSize > sedBufferSize) {
                    char *temp = (char *) realloc(sedBufferPtr, sedStringArgSize);
                    if (temp == NULL) {
                        perror("Failed realloc for sed arg string");
                        exit(EXIT_FAILURE);
                    }
                    sedBufferSize = sedStringArgSize;
                }
                snprintf(sedBufferPtr, sedBufferSize,
                        "%s%s%s%s%s%s",
                        sedSubBeginning,
                        linePtr,
                        firstSeparatorString,
                        searchFileName,
                        secondSeparatorString,
                        sedSubEnd
                        );

                execlp("sed", "sed", "-r", sedBufferPtr, NULL);
                fprintf(stderr, "failed to exec cut\n");
                exit(EXIT_FAILURE);
                break;
            default:
                close(pipeGrepSed[PIPEREAD]);
                break;
        }
    }
    /** END OF SED SECTION **/

    free(linePtr);
    free(sedBufferPtr);
    fclose(wordsStream);

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

int findIndexLastChar(char *ptr, char ch, int *len) {
    if (ptr == NULL) {
        return -1;
    }

    int indexOfLastChar = -1;
    int i;
    for (i = 0; ptr[i] != '\0'; ++i) {
        if (ptr[i] == ch) {
            indexOfLastChar = i;
        }
        if (len != NULL) {
            *len = i;
        }
    }

    return indexOfLastChar;
}

