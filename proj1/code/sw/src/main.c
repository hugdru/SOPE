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

#define CHAR_BUFFER_SIZE 128

extern char **environ;

int main(int argc, char *argv[]) {

    if (argc != 3) {
        fprintf(stderr, "%s wordsFileName searchFileName\n", argv[0]);
        return EXIT_FAILURE;
    }

    FILE *wordsStream;
    if ((wordsStream = fopen(argv[wordsFileNameIndex], "r")) == NULL) {
        perror("Failed to open the file");
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

    while ((bytesRead = getline(&linePtr, &n, wordsStream)) > 0) {

        if (linePtr[bytesRead - 1] == '\n') linePtr[bytesRead - 1] = '\0';

        pid_t pid = fork();

        switch (pid) {
            case -1:
                perror("Failed to fork");
                return EXIT_FAILURE;
            case 0:
                execlp("grep", "grep", linePtr, "-n", argv[searchFileName], NULL);
                fprintf(stderr, "failed to exec\n");
                return EXIT_FAILURE;
            default:
                break;
        }
    }

    free(linePtr);
    fclose(wordsStream);

    return EXIT_SUCCESS;
}

