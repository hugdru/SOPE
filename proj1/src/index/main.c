#include <stdio.h>

#include "FilesToSearch.h"

static void usage();

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        return EXIT_FAILURE;
    }

    FilesToSearch_t* filesToSearch = getAllTextFiles(argv[1]);
    if (filesToSearch == NULL) return EXIT_FAILURE;

    return EXIT_SUCCESS;
}

