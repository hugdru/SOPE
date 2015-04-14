#include "FilesToSearch.h"

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>

#define DEFAULT_CHAR_ARRAY_SIZE 10
#define INCREMENTOR_CHAR_ARRAY_SIZE 5

static FilesToSearch_t* FilesToSearch(void);
static FilesToSearch_t* addFileName(FilesToSearch_t * const ptr, char const * const fileName);
static FilesToSearch_t* normalizeFilesNames(FilesToSearch_t * const ptr);

FilesToSearch_t* getAllFilesNames(char const * const path) {
    if (path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    FilesToSearch_t *filesToSearch = FilesToSearch();

    DIR *directoryPtr;
    struct dirent *direntPtr;
    struct stat stat_buf;

    printf("Opening the %s directory\n", path);
    if ((directoryPtr = opendir(path)) == NULL) {
        perror("There was an error opening the directory");
        exit(EXIT_FAILURE);
    }

    if (chdir(path) != 0) {
        perror("Failed to change that folder");
        exit(EXIT_FAILURE);
    }

    while ((direntPtr = readdir(directoryPtr)) != NULL) {
        if (stat(direntPtr->d_name, &stat_buf) == -1) {
            perror("stat ERROR");
            exit(EXIT_FAILURE);
        }

        if (S_ISREG(stat_buf.st_mode)) {
            addFileName(filesToSearch, direntPtr->d_name);
        }
    }

    closedir(directoryPtr);

    normalizeFilesNames(filesToSearch);

    return filesToSearch;
}

void wipe(FilesToSearch_t * const ptr) {
    if (ptr == NULL) {
        return;
    }

    if (ptr->filesNames == NULL) {
        free(ptr);
        return;
    }

    free(ptr->filesNames);
    free(ptr);
}

static FilesToSearch_t* FilesToSearch(void) {

    FilesToSearch_t *files = (FilesToSearch_t *) malloc(sizeof(FilesToSearch_t));
    if (files == NULL) {
        perror("There was an error creating the struct FilesToSearch");
        return NULL;
    }

    files->filesNames = (const char **) malloc(sizeof(char *) * DEFAULT_CHAR_ARRAY_SIZE);
    if (files->filesNames == NULL) {
        free(files);
        perror("There was an error creating the array of file descriptors");
        return NULL;
    }
    files->allocatedSize = DEFAULT_CHAR_ARRAY_SIZE;
    files->numberOfFiles = 0;

    return files;
}

static FilesToSearch_t* addFileName(FilesToSearch_t * const ptr, char const * const fileName) {
    if (ptr == NULL || ptr->allocatedSize == 0 || fileName == NULL || ptr->numberOfFiles > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    char const **tempPtr;
    if (ptr->numberOfFiles < ptr->allocatedSize) {
        ptr->filesNames[ptr->numberOfFiles] = fileName;
        ++ptr->numberOfFiles;
    } else if (ptr->numberOfFiles == ptr->allocatedSize) {
        tempPtr = (const char **) realloc(ptr->filesNames, (INCREMENTOR_CHAR_ARRAY_SIZE + ptr->allocatedSize) * sizeof(char *));
        if (tempPtr == NULL) {
            perror("There was an error extending the array of file descriptors");
            return NULL;
        }
        ptr->filesNames = tempPtr;
        ptr->allocatedSize += INCREMENTOR_CHAR_ARRAY_SIZE;
        ptr->filesNames[ptr->numberOfFiles] = fileName;
        ++ptr->numberOfFiles;
    }
    return ptr;
}

static FilesToSearch_t* normalizeFilesNames(FilesToSearch_t * const ptr) {
    if (ptr == NULL || ptr->numberOfFiles > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    if (ptr->allocatedSize == 0) return ptr;

    if (ptr->numberOfFiles == 0) {
        ptr->allocatedSize = 0;
        free(ptr->filesNames);
        ptr->filesNames = NULL;
        return ptr;
    }

    char const **tempPtr;
    if (ptr->numberOfFiles < ptr->allocatedSize) {
        tempPtr = (const char **) realloc(ptr->filesNames, sizeof(char *) * ptr->numberOfFiles);
        if (tempPtr == NULL) {
            perror("There was an error normalizing the array of file descriptors");
            return NULL;
        }
        ptr->allocatedSize = ptr->numberOfFiles;
        ptr->filesNames = tempPtr;
    }

    return ptr;
}

