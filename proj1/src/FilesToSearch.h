#ifndef FILES_TO_SEARCH_H
#define FILES_TO_SEARCH_H

#define _XOPEN_SOURCE 700

#include <stdlib.h>

typedef struct FilesToSearch {
    ssize_t *filesDescriptors;
    size_t numberOfFiles;
    size_t allocatedSize;
} FilesToSearch_t;

FilesToSearch_t* getAllTextFiles(char const * const path);

#endif

