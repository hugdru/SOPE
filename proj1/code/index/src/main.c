#include <stdio.h>

#include "Files.h"

#include <stdbool.h>

const char defaultWordsFileName[] = "words.txt";

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Get the files to be worked on
    Files_t* files = getAllFilesNames(argv[1], defaultWordsFileName);
    if (files == NULL) {
        perror("Something went wrong finding the files");
        exit(EXIT_FAILURE);
    }

    bool exit = false;
    if (files->numberOfFiles == 0) {
        fprintf(stderr, "You must create files to be indexed\n");
        exit = true;
    }
    if (!files->foundDefaultWordsFileName) {
        fprintf(stderr, "You must create the %s file\n", defaultWordsFileName);
        exit = true;
    }
    if (exit) return EXIT_FAILURE;

    // Dynamically Allocated Array
    wipe(files);

    return EXIT_SUCCESS;
}

