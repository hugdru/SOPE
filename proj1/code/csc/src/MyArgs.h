#ifndef FILES_TO_SEARCH_H_
#define FILES_TO_SEARCH_H_

#define _XOPEN_SOURCE 700

#include <stdlib.h>

typedef struct MyArgs {
    char **args;
    size_t allocatedSize;
    size_t size;
} MyArgs_t;

MyArgs_t* fillMyArgs(char const * const dirPath, char * const programName);
void wipe(MyArgs_t * const ptr);

#endif

