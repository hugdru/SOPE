#include "FilesToSearch.h"

#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#define DEFAULT_ARRAY_SIZE 10
#define INCREMENTOR_ARRAY_SIZE 5

static FilesToSearch_t* FilesToSearch();
static FilesToSearch_t* addFileDescriptor(FilesToSearch_t * const ptr, ssize_t const fileDescriptor);

FilesToSearch_t* getAllTextFiles(char const * const path) {
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

    // One has to change to the dir before calling lstat, or just append path + direntp->d_name
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
            ssize_t fileDescriptor = open(direntPtr->d_name, O_RDONLY);
            if (fileDescriptor <= 0) exit(EXIT_FAILURE);
            addFileDescriptor(filesToSearch, fileDescriptor);
        }
    }

    closedir(directoryPtr);

    return filesToSearch;
}

static FilesToSearch_t* FilesToSearch() {

    FilesToSearch_t *files = (FilesToSearch_t *) malloc(sizeof(FilesToSearch_t));
    if (files == NULL) {
        perror("There was an error creating the struct FilesToSearch");
        return NULL;
    }

    files->filesDescriptors = (ssize_t *) malloc(sizeof(ssize_t) * DEFAULT_ARRAY_SIZE);
    if (files->filesDescriptors == NULL) {
        free(files);
        perror("There was an error creating the array of file descriptors");
        return NULL;
    }
    files->allocatedSize = DEFAULT_ARRAY_SIZE;
    files->numberOfFiles = 0;

    return files;
}

static FilesToSearch_t* addFileDescriptor(FilesToSearch_t * const ptr, ssize_t const fileDescriptor) {
    if (ptr == NULL || ptr->allocatedSize <= 0 || fileDescriptor < 0 || ptr->numberOfFiles > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    ssize_t *tempPtr;
    if (ptr->numberOfFiles < ptr->allocatedSize) {
        ptr->filesDescriptors[ptr->numberOfFiles++] = fileDescriptor;
    } else if (ptr->numberOfFiles == ptr->allocatedSize) {
        tempPtr = realloc(ptr->filesDescriptors, (INCREMENTOR_ARRAY_SIZE + ptr->allocatedSize) * sizeof(ssize_t));
        if (tempPtr == NULL) {
            perror("There was an error extending the array of file descriptors");
            return NULL;
        }
        ptr->filesDescriptors = tempPtr;
        ptr->allocatedSize += INCREMENTOR_ARRAY_SIZE;
        ptr->filesDescriptors[ptr->numberOfFiles++] = fileDescriptor;
    }
    return ptr;
}

static FilesToSearch_t* normalizeFilesDescriptors(FilesToSearch_t *ptr) {
    if (ptr == NULL || ptr->allocatedSize <= 0 || ptr->numberOfFiles > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    ssize_t *tempPtr;
    if (ptr->numberOfFiles < ptr->allocatedSize) {
        tempPtr = (ssize_t *) realloc(ptr->filesDescriptors, sizeof(ssize_t) * ptr->numberOfFiles);
        if (tempPtr == NULL) {
            perror("There was an error normalizing the array of file descriptors");
            return NULL;
        }
        ptr->allocatedSize = ptr->numberOfFiles;
        ptr->filesDescriptors = tempPtr;
    }

    return ptr;
}

