#ifndef FILES_TO_SEARCH_H
#define FILES_TO_SEARCH_H

#define _XOPEN_SOURCE 700

#include <stdlib.h>

typedef struct FilesToSearch {
    char const **filesNames;
    size_t numberOfFiles;
    size_t allocatedSize;
} FilesToSearch_t;

FilesToSearch_t* getAllFilesNames(char const * const path);
void wipe(FilesToSearch_t * const ptr);

#endif

