#include <stdio.h>

#include "Files.h"

#include <stdbool.h>

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    Files_t* files = getAllFilesNames(argv[1]);
    if (files == NULL) {
        return EXIT_FAILURE;
    }

    bool exit = false;
    if (files->numberOfFiles == 0) {
        fprintf(stderr, "You must create files to be indexed\n");
        exit = true;
    }
    if (files->wordsFilename == NULL) {
        fprintf(stderr, "You must create the %s file\n", defaultWordsFileName);
        exit = true;
    }
    if (exit) return EXIT_FAILURE;

    for (size_t index = 0; index < files->numberOfFiles; ++index) {
        printf("%s\n", files->filesNamesToSearch[index]);
    }

    printf("%s\n", files->wordsFilename);
    printf("%zd, %zd\n", files->numberOfFiles, files->allocatedSize);

    wipe(files);

    return EXIT_SUCCESS;
}

