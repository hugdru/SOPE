#define _XOPEN_SOURCE 700

#include "MyArgs.h"

#include <stdio.h>
#include <unistd.h>

#define DIRPATHINDEX 1
#define PIPEREAD 0
#define PIPEWRITE 1

#define CATARGVDEFAULTSIZE 20

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Fills the cats arguments, cat + files + NULL
    // Internally calls some functions to create, add and normalize a struct MyArgs
    MyArgs_t *myCatArgs;
    if ((myCatArgs = fillMyArgs(argv[DIRPATHINDEX], "cat")) == NULL) {
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
        execvp("cat", myCatArgs->args);
        fprintf(stderr, "failed to exec cat\n");
        goto cleanUp;
    default:
        // I can't close the pipeCatSort[PIPEREAD] because the OS will see that there are
        // no processes conected to the read end and will generate a SIGPIPE signal
        close(pipeCatSort[PIPEWRITE]);
    }

    pid_t pidForkSort = fork();

    switch (pidForkSort) {
    case -1:
        perror("sort fork failed");
        goto cleanUp;
    case 0:
        dup2(pipeCatSort[PIPEREAD], STDIN_FILENO);
        execlp("sort", "sort", "-b", NULL);
        fprintf(stderr, "failed to exec sort\n");
        goto cleanUp;
    default:
        close(pipeCatSort[PIPEREAD]);
    }

    exit(EXIT_SUCCESS);
cleanUp:
    wipe(myCatArgs);
    exit(EXIT_FAILURE);
}

