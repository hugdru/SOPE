#ifndef FILES_TO_SEARCH_H_
#define FILES_TO_SEARCH_H_

#define _XOPEN_SOURCE 700

#include <stdlib.h>

typedef struct MyArgs {
    char **args;
    size_t allocatedSize;
    size_t size;
} MyArgs_t;

// Find all the files in a dir and build the argv for a certain program
// programName, file1, file2, NULL
MyArgs_t* fillMyArgs(char const * const dirPath, char const * const programName);
// Clean any dynamically alocated memory
void wipe(MyArgs_t * const ptr);

#endif

