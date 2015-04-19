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
#define nKids 3

// sed substitute string that converts grep output to what we want
// it ignores the path part of the file and the extension, and
// puts everything in the format we want. Everything line by line.
const char sedMagic[] = "s@^(.*/){0,1}([^:.]*)[^:]*:([^:]*):(.*)$@\\4: \\2-\\3@i";

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "%s wordsFileName searchFileName\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct stat stat_buf;
    int returned = stat(argv[wordsFilePathIndex], &stat_buf);
    if (returned != 0 || !S_ISREG(stat_buf.st_mode)) {
        errno = EINVAL;
        perror("bad words file path");
        exit(EXIT_FAILURE);
    }

    returned = stat(argv[searchFilePathIndex], &stat_buf);
    if (returned != 0 || !S_ISREG(stat_buf.st_mode)) {
        errno = EINVAL;
        perror("bad search file path");
        exit(EXIT_FAILURE);
    }

    // sw    grep ->[]- uniq ->[]- sed ->[]--(stdout)
    /** GREP SECTION **/
    int pipeGrepUniq[2];
    if (pipe(pipeGrepUniq) < 0) {
        perror("Could not create a pipe grep -> uniq");
        exit(EXIT_FAILURE);
    }

    pid_t pidForkGrep = fork();

    switch (pidForkGrep) {
        case -1:
            perror("Failed to fork grep");
            exit(EXIT_FAILURE);
            break;
        case 0:
            close(pipeGrepUniq[PIPEREAD]);
            dup2(pipeGrepUniq[PIPEWRITE], STDOUT_FILENO);
            execlp("grep", "grep", argv[searchFilePathIndex], "-n", "-H", "-w", "-o", "-f", argv[wordsFilePathIndex], NULL);
            fprintf(stderr, "failed to exec grep\n");
            exit(EXIT_FAILURE);
            break;
        default:
            // I can't close the pipeGrepCut[PIPEREAD] because the OS will see that there are
            // no processes conected to the read end and will generate a SIGPIPE signal
            close(pipeGrepUniq[PIPEWRITE]);
            break;
    }
    /** END OF GREP SECTION **/

    /** START OF UNIQ SECTION **/
    int pipeUniqSed[2];
    if (pipe(pipeUniqSed) < 0) {
        perror("Could not create a pipe uniq -> sed");
        exit(EXIT_FAILURE);
    }

    pid_t pidForkUniq = fork();

    switch (pidForkUniq) {
        case -1:
            perror("Failed to fork uniq");
            exit(EXIT_FAILURE);
            break;
        case 0:
            dup2(pipeGrepUniq[PIPEREAD], STDIN_FILENO);
            close(pipeUniqSed[PIPEREAD]);
            dup2(pipeUniqSed[PIPEWRITE], STDOUT_FILENO);
            execlp("uniq", "uniq", NULL);
            fprintf(stderr, "failed to exec uniq\n");
            exit(EXIT_FAILURE);
            break;
        default:
            close(pipeGrepUniq[PIPEREAD]);
            close(pipeUniqSed[PIPEWRITE]);
            break;
    }
    /** END OF UNIQ SECTION **/

    /** START OF SED SECTION **/
    pid_t pidForkSed = fork();

    switch (pidForkSed) {
        case -1:
            perror("Failed to fork sed");
            exit(EXIT_FAILURE);
            break;
        case 0:
            close(pipeUniqSed[PIPEWRITE]);
            dup2(pipeUniqSed[PIPEREAD], STDIN_FILENO);
            execlp("sed", "sed", "-r", sedMagic, NULL);
            fprintf(stderr, "failed to exec sed\n");
            exit(EXIT_FAILURE);
            break;
        default:
            close(pipeGrepUniq[PIPEREAD]);
            break;
    }
    /** END OF SED SECTION **/

    int i = 0;
    while (i != nKids) {
        int status;
        if (wait(&status) == -1) {
            fprintf(stderr, "There was an error waiting for children\n");
            exit(EXIT_FAILURE);
        }

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "Something wrong with a child\n");
                exit(EXIT_FAILURE);
            }
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child killed by signal\n");
            exit(EXIT_FAILURE);
        }
        ++i;
    }

    exit(EXIT_SUCCESS);
}

