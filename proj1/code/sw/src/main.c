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

#define wordsFileNameIndex 1
#define searchFileNameIndex 2
#define PIPEREAD 0
#define PIPEWRITE 1

#define CHAR_BUFFER_SIZE 128

const char sedSubBeginning[] = "s/([0-9]*).*/";
const char sedSubEnd[] = "\\1/";
const char firstSeparatorString[] = " : ";
const char secondSeparatorString[] = "-";
const size_t sedConstantLen =
    (sizeof(sedSubBeginning) + sizeof(sedSubEnd) +
     sizeof(firstSeparatorString) + sizeof(secondSeparatorString) - 4) / sizeof(sedSubBeginning[0]);

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
    int result = stat(argv[searchFileNameIndex], &stat_buf);
    if (result != 0 || !S_ISREG(stat_buf.st_mode)) {
        errno = EINVAL;
        perror("bad search file name");
        return EXIT_FAILURE;
    }

    // fileName extension ignore rest of string
    char *charPtr = argv[searchFileNameIndex];
    size_t searchFileNameNotExtLen = 0;
    for (size_t i = 0; charPtr != NULL && charPtr[i] != '\0'; ++i) {
        if (charPtr[i] == '.') {
            searchFileNameNotExtLen = i;
        }
    }

    char *searchFileNameNotExt = charPtr;
    if (searchFileNameNotExtLen != 0) {
        searchFileNameNotExt = (char *) malloc(sizeof(char) * (searchFileNameNotExtLen + 1));
        snprintf(searchFileNameNotExt, searchFileNameNotExtLen + 1, "%s", argv[searchFileNameIndex]);
    }

    puts(searchFileNameNotExt);

    // Buffer for getline, if it is not enoguh it reallocs
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
                execlp("grep", "grep", linePtr, "-n", argv[searchFileNameIndex], NULL);
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
                size_t sedStringArgSize = sedConstantLen + searchFileNameNotExtLen + strlen(linePtr) + 1;
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
                        searchFileNameNotExt,
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
    fclose(wordsStream);

    return EXIT_SUCCESS;
}

