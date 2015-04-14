#ifndef FILES_TO_SEARCH_H
#define FILES_TO_SEARCH_H

#define _XOPEN_SOURCE 700

#include <stdlib.h>

const char defaultWordsFileName[] = "words.txt";

typedef struct Files {
    char const *wordsFilename;
    char const **filesNamesToSearch;
    size_t numberOfFiles;
    size_t allocatedSize;
} Files_t;

Files_t* getAllFilesNames(char const * const path);
void wipe(Files_t * const ptr);

#endif

