#ifndef FILES_TO_SEARCH_H_
#define FILES_TO_SEARCH_H_

#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <stdint.h>

#define true 1
#define false 0

typedef struct Files {
    char const **filesNamesToSearch;
    char const *wordsFileName;
    size_t numberOfFiles;
    size_t allocatedSize;
    size_t foundDefaultWordsFileName;
} Files_t;

// Navigate in a directory and store in filesNamesToSearch all the filesNames of the files
// present in that directory. Calls other functions to create, add and normalize a Files struct.
Files_t* getAllFilesNames(char const * const path, char const * const defaultWordsFileName);
// We freed the array of pointers but not the strings because its the Kernels job to do that
// Because those strings get returned by it, if we clean it all hell breaks loose
void wipe(Files_t * const ptr);

#endif

