#define _XOPEN_SOURCE 700

#include "MyArgs.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#define DIRPATHINDEX 1
#define PIPEREAD 0
#define PIPEWRITE 1
#define NKIDS 3

char catString[] = "cat";
char sedMagic[] = ":a;$!N;s/^([^:]*)(.*)\\n\\1: (.*)$/\\1\\2, \\3/;ta;P;D";

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Fills the cats arguments, cat + files + NULL
    // Internally calls some functions to create, add and normalize a struct MyArgs
    MyArgs_t *myCatArgs;
    if ((myCatArgs = fillMyArgs(argv[DIRPATHINDEX], catString)) == NULL) {
        perror("failed to fill my args for cat");
        exit(EXIT_FAILURE);
    }

    int pipeCatSort[2];
    if (pipe(pipeCatSort) == -1) {
        perror("pipe cat->sort failed");
        goto cleanUp;
    }

    pid_t pidForkCat = fork();

    switch (pidForkCat) {
    case -1:
        perror("cat fork failed");
        goto cleanUp;
    case 0:
        close(pipeCatSort[PIPEREAD]);
        dup2(pipeCatSort[PIPEWRITE], STDOUT_FILENO);
        execvp(catString, myCatArgs->args);
        fprintf(stderr, "failed to exec cat\n");
        goto cleanUp;
    default:
        // I can't close the pipeCatSort[PIPEREAD] because the OS will see that there are
        // no processes conected to the read end and will generate a SIGPIPE signal
        close(pipeCatSort[PIPEWRITE]);
    }

    int pipeSortSed[2];
    if (pipe(pipeSortSed) == -1) {
        perror("pipe sort->sed failed");
        goto cleanUp;
    }

    pid_t pidForkSort = fork();

    switch (pidForkSort) {
    case -1:
        perror("sort fork failed");
        goto cleanUp;
    case 0:
        dup2(pipeCatSort[PIPEREAD], STDIN_FILENO);
        close(pipeSortSed[PIPEREAD]);
        dup2(pipeSortSed[PIPEWRITE], STDOUT_FILENO);
        execlp("sort", "sort", "-b", NULL);
        fprintf(stderr, "failed to exec sort\n");
        goto cleanUp;
    default:
        close(pipeCatSort[PIPEREAD]);
        // I can't close the pipeSortSed[PIPEREAD] because the OS will see that there are
        // no processes conected to the read end and will generate a SIGPIPE signal
        close(pipeSortSed[PIPEWRITE]);
    }

    pid_t pidForkSed = fork();

    switch (pidForkSed) {
    case -1:
        perror("sed fork failed");
        goto cleanUp;
    case 0:
        dup2(pipeSortSed[PIPEREAD], STDIN_FILENO);
        close(pipeSortSed[PIPEWRITE]);
        execlp("sed", "sed", "-r", sedMagic, NULL);
        fprintf(stderr, "failed to exec sed\n");
        goto cleanUp;
    default:
        close(pipeSortSed[PIPEREAD]);
    }

    size_t i = 0;
    while (i < NKIDS) {
        int status;
        if (wait(&status) == -1) {
            fprintf(stderr, "There was an error waiting for children\n");
            goto cleanUp;
        }

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "Something wrong with a child\n");
                goto cleanUp;
            }
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child killed by signal\n");
            goto cleanUp;
        }
        ++i;
    }

    exit(EXIT_SUCCESS);
cleanUp:
    wipe(myCatArgs);
    exit(EXIT_FAILURE);
}

