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

const char sedMagic[] = "s@^(.*/){0,1}([^:.]*)[^:]*:([^:]*):(.*)$@\\4 : \\2-\\3@i";

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "%s wordsFileName searchFileName\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat stat_buf;
    int returned = stat(argv[wordsFilePathIndex], &stat_buf);
    if (returned != 0 || !S_ISREG(stat_buf.st_mode)) {
        errno = EINVAL;
        perror("bad search file name");
        exit(EXIT_FAILURE);
    }

    returned = stat(argv[searchFilePathIndex], &stat_buf);
    if (returned != 0 || !S_ISREG(stat_buf.st_mode)) {
        errno = EINVAL;
        perror("bad search file name");
        exit(EXIT_FAILURE);
    }

    // sw    grep ->[]- sed ->[]--(stdout)
    /** GREP SECTION **/
    int pipeGrepSed[2];
    if (pipe(pipeGrepSed) < 0) {
        perror("Could not create a pipe cut -> sw");
        exit(EXIT_FAILURE);
    }

    pid_t pidForkGrep = fork();

    switch (pidForkGrep) {
        case -1:
            perror("Failed to fork cut");
            exit(EXIT_FAILURE);
            break;
        case 0:
            close(pipeGrepSed[PIPEREAD]);
            dup2(pipeGrepSed[PIPEWRITE], STDOUT_FILENO);
            execlp("grep", "grep", argv[searchFilePathIndex], "-i", "-n", "-H", "-o", "-f", argv[wordsFilePathIndex], NULL);
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

    pid_t pidForkSed = fork();

    switch (pidForkSed) {
        case -1:
            perror("Failed to fork grep");
            exit(EXIT_FAILURE);
            break;
        case 0:
            dup2(pipeGrepSed[PIPEREAD], STDIN_FILENO);
            execlp("sed", "sed", "-r", sedMagic, NULL);
            fprintf(stderr, "failed to exec cut\n");
            exit(EXIT_FAILURE);
            break;
        default:
            close(pipeGrepSed[PIPEREAD]);
            break;
    }
    /** END OF SED SECTION **/

    exit(EXIT_SUCCESS);
}

