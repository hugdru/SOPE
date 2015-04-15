#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#define wordsFileNameIndex 1
#define searchFileName 2
#define PIPEREAD 0
#define PIPEWRITE 1

#define CHAR_BUFFER_SIZE 128

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "%s wordsFileName searchFileName\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *wordsStream;
    if ((wordsStream = fopen(argv[wordsFileNameIndex], "r")) == NULL) {
        perror("Can't find the file with the words");
        return EXIT_FAILURE;
    }

    struct stat stat_buf;
    int result = stat(argv[searchFileName], &stat_buf);
    if (result != 0 || !S_ISREG(stat_buf.st_mode)) {
        errno = EINVAL;
        perror("bad search file name");
        return EXIT_FAILURE;
    }

    size_t n = CHAR_BUFFER_SIZE;
    char *linePtr = (char *) malloc(sizeof(char) * CHAR_BUFFER_SIZE);
    ssize_t bytesRead;

    // sw    grep ->[]- sed ->[]--(stdout)
    while ((bytesRead = getline(&linePtr, &n, wordsStream)) > 0) {

        if (linePtr[bytesRead - 1] == '\n') linePtr[bytesRead - 1] = '\0';

        /** GREP SECTION **/
        int pipeGrepSed[2];
        if (pipe(pipeGrepSed) < 0) {
            perror("Could not create a pipe cut -> sw");
            exit(EXIT_FAILURE);
        }

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
                execlp("grep", "grep", linePtr, "-n", argv[searchFileName], NULL);
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
        /** START OF CUT SECTION **/

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

                execlp("sed", "sed", "-r", "-d:", NULL);
                fprintf(stderr, "failed to exec cut\n");
                exit(EXIT_FAILURE);
                break;
            default:
                close(pipeGrepSed[PIPEREAD]);
                break;
        }
        break;

    }

    free(linePtr);
    fclose(wordsStream);

    return EXIT_SUCCESS;
}

