#include <stdio.h>

#include "FilesToSearch.h"

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FilesToSearch_t* filesToSearch = getAllFilesNames(argv[1]);
    if (filesToSearch == NULL) return EXIT_FAILURE;

    for (size_t index = 0; index < filesToSearch->numberOfFiles; ++index) {
        printf("%s\n", filesToSearch->filesNames[index]);
    }

    printf("%zd, %zd\n", filesToSearch->numberOfFiles, filesToSearch->allocatedSize);

    // free mallocs
    wipe(filesToSearch);

    return EXIT_SUCCESS;
}

