#include "Files.h"

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>

#define DEFAULT_CHAR_ARRAY_SIZE 10
#define INCREMENTOR_CHAR_ARRAY_SIZE 5

// Creates a Files struct
static Files_t* Files(char const * const defaultWordsFileName, char const * const defaultIndexFileName);
// Adds a fileName to the struct
static Files_t* addFileName(Files_t * const ptr, char const * const fileName);
// Shorten the buffer to the needed size
static Files_t* normalizeFilesNames(Files_t * const ptr);

Files_t* getAllFilesNames(char const * const dirPath, char const * const defaultWordsFileName,
        char const * const defaultIndexFileName) {
    if (dirPath == NULL || defaultWordsFileName == NULL) {
        errno = EINVAL;
        return NULL;
    }

    Files_t *filesToSearch = Files(defaultWordsFileName, defaultIndexFileName);

    DIR *directoryPtr;
    struct dirent *direntPtr;
    struct stat stat_buf;

    if ((directoryPtr = opendir(dirPath)) == NULL) {
        perror("There was an error opening the directory");
        exit(EXIT_FAILURE);
    }

    if (chdir(dirPath) != 0) {
        perror("Failed to change that folder");
        exit(EXIT_FAILURE);
    }

    while ((direntPtr = readdir(directoryPtr)) != NULL) {
        if (stat(direntPtr->d_name, &stat_buf) == -1) {
            perror("stat ERROR");
            exit(EXIT_FAILURE);
        }

        if (S_ISREG(stat_buf.st_mode)) {
            if (strcasecmp(direntPtr->d_name, filesToSearch->indexFileName) == 0) continue;
            addFileName(filesToSearch, direntPtr->d_name);
        }
    }

    closedir(directoryPtr);

    normalizeFilesNames(filesToSearch);

    return filesToSearch;
}

void wipe(Files_t * const ptr) {
    if (ptr == NULL) {
        return;
    }

    if (ptr->filesNamesToSearch == NULL) {
        free(ptr);
        return;
    }

    for (size_t i = 0; i < ptr->numberOfFiles; ++i) {
        if (ptr->filesNamesToSearch[i] != NULL) {
            free(ptr->filesNamesToSearch[i]);
        }
    }

    free(ptr->filesNamesToSearch);
    free(ptr);
}

static Files_t* Files(char const * const defaultWordsFileName, char const * const defaultIndexFileName) {

    Files_t *files = (Files_t *) malloc(sizeof(Files_t));
    if (files == NULL) {
        perror("There was an error creating the struct Files");
        return NULL;
    }

    files->filesNamesToSearch = (char **) malloc(sizeof(char *) * DEFAULT_CHAR_ARRAY_SIZE);
    if (files->filesNamesToSearch == NULL) {
        free(files);
        perror("There was an error creating the array of file names");
        return NULL;
    }
    files->allocatedSize = DEFAULT_CHAR_ARRAY_SIZE;
    files->numberOfFiles = 0;
    files->foundDefaultWordsFileName = false;
    files->wordsFileName = defaultWordsFileName;
    files->indexFileName = defaultIndexFileName;

    return files;
}

static Files_t* addFileName(Files_t * const ptr, char const * const fileName) {
    if (ptr == NULL || ptr->allocatedSize == 0 || fileName == NULL || ptr->numberOfFiles > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    if (!ptr->foundDefaultWordsFileName) {
        if (strcasecmp(fileName, ptr->wordsFileName) == 0) {
            ptr->foundDefaultWordsFileName = true;
            return ptr;
        }
    }

    char *duplicateFileName;
    char **tempPtr;
    if (ptr->numberOfFiles < ptr->allocatedSize) {
        duplicateFileName = strdup(fileName);
        if (duplicateFileName == NULL) {
            perror("Failed to duplicate file Name");
            return NULL;
        }
        ptr->filesNamesToSearch[ptr->numberOfFiles] = duplicateFileName;
        ++ptr->numberOfFiles;
    } else if (ptr->numberOfFiles == ptr->allocatedSize) {
        tempPtr = (char **) realloc(ptr->filesNamesToSearch, (INCREMENTOR_CHAR_ARRAY_SIZE + ptr->allocatedSize) * sizeof(char *));
        if (tempPtr == NULL) {
            perror("There was an error extending the array of file names");
            return NULL;
        }
        duplicateFileName = strdup(fileName);
        if (duplicateFileName == NULL) {
            perror("Failed to duplicate file Name");
            return NULL;
        }
        ptr->filesNamesToSearch = tempPtr;
        ptr->allocatedSize += INCREMENTOR_CHAR_ARRAY_SIZE;
        ptr->filesNamesToSearch[ptr->numberOfFiles] = duplicateFileName;
        ++ptr->numberOfFiles;
    }
    return ptr;
}

static Files_t* normalizeFilesNames(Files_t * const ptr) {
    if (ptr == NULL || ptr->numberOfFiles > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    if (ptr->allocatedSize == 0) return ptr;

    if (ptr->numberOfFiles == 0) {
        ptr->allocatedSize = 0;
        free(ptr->filesNamesToSearch);
        ptr->filesNamesToSearch = NULL;
        return ptr;
    }

    char **tempPtr;
    if (ptr->numberOfFiles < ptr->allocatedSize) {
        tempPtr = (char **) realloc(ptr->filesNamesToSearch, sizeof(char *) * ptr->numberOfFiles);
        if (tempPtr == NULL) {
            perror("There was an error normalizing the array of file names");
            return NULL;
        }
        ptr->allocatedSize = ptr->numberOfFiles;
        ptr->filesNamesToSearch = tempPtr;
    }

    return ptr;
}

