#ifndef FILES_TO_SEARCH_H_
#define FILES_TO_SEARCH_H_

#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <stdint.h>

#define true 1
#define false 0

typedef struct Files {
    char const **filesNamesToSearch;
    char *wordsFileName;
    size_t numberOfFiles;
    size_t allocatedSize;
    size_t foundDefaultWordsFileName;
} Files_t;

Files_t* getAllFilesNames(char const * const path, char const * const defaultWordsFileName);
void wipe(Files_t * const ptr);

#endif

