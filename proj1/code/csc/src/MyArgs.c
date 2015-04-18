#include "MyArgs.h"

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

// Creats a MyArgs struct
static MyArgs_t* MyArgs(void);
// Adds a fileName to MyArgs
static MyArgs_t* addArg(MyArgs_t * const ptr, char * const arg);
// Shortens the buffer to size + 1, +1 because of the NULL
static MyArgs_t* normalize(MyArgs_t * const ptr);

MyArgs_t* fillMyArgs(char const * const dirPath, char * const programName) {
    if (dirPath == NULL) {
        errno = EINVAL;
        return NULL;
    }

    MyArgs_t *myArgs = MyArgs();
    if (myArgs == NULL) {
        return NULL;
    }
    addArg(myArgs, programName);

    DIR *directoryPtr;
    struct dirent *direntPtr;
    struct stat stat_buf;

    if ((directoryPtr = opendir(dirPath)) == NULL) {
        perror("There was an error opening the directory");
        goto cleanUp;
    }

    if (chdir(dirPath) != 0) {
        perror("Failed to change to that folder");
        goto cleanUp;
    }

    while ((direntPtr = readdir(directoryPtr)) != NULL) {
        if (stat(direntPtr->d_name, &stat_buf) == -1) {
            goto cleanUp;
        }

        if (S_ISREG(stat_buf.st_mode)) {
            addArg(myArgs, direntPtr->d_name);
        }
    }

    closedir(directoryPtr);

    normalize(myArgs);

    return myArgs;

cleanUp:
    free(myArgs);
    return NULL;
}

void wipe(MyArgs_t * const ptr) {
    if (ptr == NULL) {
        return;
    }

    if (ptr->args == NULL) {
        free(ptr);
        return;
    }

    free(ptr->args);
    free(ptr);
}

static MyArgs_t* MyArgs(void) {

    MyArgs_t *myArgs = (MyArgs_t *) malloc(sizeof(MyArgs_t));
    if (myArgs == NULL) {
        perror("There was an error creating the struct MyArgs");
        return NULL;
    }

    myArgs->args = (char **) malloc(sizeof(char *) * DEFAULT_CHAR_ARRAY_SIZE);
    if (myArgs->args == NULL) {
        free(myArgs);
        perror("There was an error creating the array of char pointers");
        return NULL;
    }
    myArgs->allocatedSize = DEFAULT_CHAR_ARRAY_SIZE;
    myArgs->size = 0;

    return myArgs;
}

static MyArgs_t* addArg(MyArgs_t * const ptr, char * const arg) {
    if (ptr == NULL || ptr->allocatedSize == 0 || arg == NULL || ptr->size > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    char **tempPtr;
    if (ptr->size < ptr->allocatedSize) {
        ptr->args[ptr->size] = arg;
        ++ptr->size;
    } else if (ptr->size == ptr->allocatedSize) {
        tempPtr = (char **) realloc(ptr->args, (INCREMENTOR_CHAR_ARRAY_SIZE + ptr->allocatedSize) * sizeof(char *));
        if (tempPtr == NULL) {
            perror("There was an error extending the array of char pointers");
            return NULL;
        }
        ptr->args = tempPtr;
        ptr->allocatedSize += INCREMENTOR_CHAR_ARRAY_SIZE;
        ptr->args[ptr->size] = arg;
        ++ptr->size;
    }
    return ptr;
}

static MyArgs_t* normalize(MyArgs_t * const ptr) {
    if (ptr == NULL || ptr->size > ptr->allocatedSize) {
        errno = EINVAL;
        return NULL;
    }

    if (ptr->allocatedSize == 0) return ptr;

    if (ptr->size == 0) {
        ptr->allocatedSize = 0;
        free(ptr->args);
        ptr->args = NULL;
        return ptr;
    }

    char **tempPtr;
    if (ptr->size < ptr->allocatedSize) {
        tempPtr = (char **) realloc(ptr->args, sizeof(char *) * (++ptr->size));
        if (tempPtr == NULL) {
            perror("There was an error normalizing the array of char pointers");
            return NULL;
        }
        ptr->args[ptr->size - 1] = NULL;
        ptr->allocatedSize = ptr->size;
        ptr->args = tempPtr;
    }

    return ptr;
}

