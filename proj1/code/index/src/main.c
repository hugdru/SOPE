#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <ftw.h>
#include <errno.h>
#include <stdbool.h>

#include "Files.h"

#define DIRPATHINDEX 1
#define INDEXPATHINDEX 2
#define PIPEREAD 0
#define PIPEWRITE 1

long nChildProcesses = 0;

// Name of the file which holds the words
const char defaultWordsFileName[] = "words.txt";
const char defaultIndexFileName[] = "index.txt";

// "/tmp/index-pid/fileName"
// Incomplete(missing -pid) dir where we will place the sw result files
const char tempFilePathDir[] = "/tmp/index";
// Size of the buffer which holds the complete temp directory path
const size_t tempFilePathDirBufSize = (sizeof(tempFilePathDir) / sizeof(tempFilePathDir[0])) + (1 + 7 + 1) * sizeof(char);

// Handler to reap the children and to decrement the
// number of indexe's active children
void childHandler(int signo);

// Function which removes the temp file dir
void removeTempDir(char *dirPath);
// Handler for the ftw function which we use
// to recursively delete files inside a folder
int ftwHandler(const char *fpath, const struct stat *sb, int typeflag);

int main(int argc, char *argv[]) {

    // Max number of processes index can have at any given time
    const long maxChildProcesses = sysconf(_SC_CHILD_MAX) / 4;

    if (argc != 2 && argc != 3) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
        fprintf(stderr, "%s <directory> <IndexPath>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Gets the original working directory it is needed because
    // the helper function getAllfilesNames changes dir
    char *originalWd = getcwd(NULL, 0);
    if (originalWd == NULL) {
        perror("can't get working directory");
        exit(EXIT_FAILURE);
    }

    // Fills the files struct with all the file names in a given directory
    // This helper function is smart enough to allocate only the needed space
    // It changes dir inside it so we must have that in mind
    // It ignores the defaultIndexFileName
    Files_t* files = getAllFilesNames(argv[DIRPATHINDEX], defaultWordsFileName, defaultIndexFileName);
    if (files == NULL) {
        perror("Something went wrong finding the files");
        free(originalWd);
        exit(EXIT_FAILURE);
    }

    int escape = false;
    if (files->numberOfFiles == 0) {
        fprintf(stderr, "You must create files to be indexed\n");
        escape = true;
    }
    if (!files->foundDefaultWordsFileName) {
        fprintf(stderr, "You must create the %s file\n", defaultWordsFileName);
        escape = true;
    }
    if (escape) {
        free(originalWd);
        wipe(files);
        exit(EXIT_FAILURE);
    }

    /** INSTALL SIGCHLD HANDLER AND FILL SUSPEND MASK**/
    // This is needed to void zombies and also makes it
    // possible to limit how many processes index can have
    // at any given time, so we don't fill the pids if
    // there are too many files to search
    struct sigaction sigact;
    sigact.sa_handler = childHandler;
    sigemptyset(&sigact.sa_mask);
    // Do not receive job control notification from child
    sigact.sa_flags = SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
        perror("There was an error installing the SIGCHLD handler");
        free(originalWd);
        wipe(files);
        exit(EXIT_FAILURE);
    }

    sigset_t sigmask;
    if (sigfillset(&sigmask) == -1 || sigdelset(&sigmask, SIGCHLD) == -1) {
        perror("Failed setting the mask for SIGCHLD suspend if nChildProcesses == maxChildProcesses");
        free(originalWd);
        wipe(files);
        exit(EXIT_FAILURE);
    }
    /** END OF INSTALL SIGCHLD HANDLER AND FILL SUSPEND MASK**/

    // Allocate tempory space for the temp directory where I will place the results of each sw
    // Folder has the format /tmp/index-pid/
    // This way we can have many index processes working at the same time
    char *tempFilePath = (char *) malloc(tempFilePathDirBufSize * sizeof(char));
    int tempFilePathLen = snprintf(tempFilePath, tempFilePathDirBufSize, "%s-%d/", tempFilePathDir, getpid());
    if (tempFilePathLen < 0) {
        perror("Failed to create string temp file path dir");
        free(originalWd);
        wipe(files);
        exit(EXIT_FAILURE);
    }

    // Create the folder where I will place the temporary results of each sw
    if (mkdir(tempFilePath, 0770) == -1) {
        perror("Failed to create temp file folder");
        if (errno == EEXIST) {
            fprintf(stderr, "Remove the %s directory manually\n", tempFilePath);
        }
        free(originalWd);
        free(tempFilePath);
        wipe(files);
        exit(EXIT_FAILURE);
    }

    // Change dir to temporary files dir
    if (chdir(tempFilePath) == -1) {
        perror("Failed to change to temporary files dir");
        free(originalWd);
        free(tempFilePath);
        wipe(files);
        exit(EXIT_FAILURE);
    }

    int tempFilesDescriptors[files->numberOfFiles];
    for (size_t t = 0; t < files->numberOfFiles; ++t) {
        tempFilesDescriptors[t] =
            open(
                files->filesNamesToSearch[t],
                O_WRONLY | O_CREAT | O_EXCL | O_SYNC,
                0700
            );

        if (tempFilesDescriptors[t] == -1) {
            perror("Failed to create a temp file");
            for (size_t i = 0; i < t; ++i) {
                if (close(tempFilesDescriptors[i]) == -1) {
                    perror("Failed closing a temp file due to error");
                }
            }
            goto cleanUpParent;
        }
    }
    if (argv[DIRPATHINDEX][0] != '/') {
        if (chdir(originalWd) == -1) {
            perror("failed to change to original working directory");
            goto cleanUpParent;
        }
        if (chdir(argv[DIRPATHINDEX]) == -1) {
            perror("failed to change to files directory");
            goto cleanUpParent;
        }
    } else {
        if (chdir(argv[DIRPATHINDEX]) == -1) {
            perror("failed to change to files directory");
            goto cleanUpParent;
        }
    }

    size_t fileDescriptorIndex;
    for (fileDescriptorIndex = 0; fileDescriptorIndex < files->numberOfFiles; ++fileDescriptorIndex) {

        if (nChildProcesses == maxChildProcesses) sigsuspend(&sigmask);
        ++nChildProcesses;

        pid_t pidSw = fork();

        switch (pidSw) {
        case -1: {
            perror("Failed to create a sw child process");
            goto cleanUpParent;
        }
        case 0: {
            if (dup2(tempFilesDescriptors[fileDescriptorIndex], STDOUT_FILENO) == -1) {
                perror("Failed to redirect child stdout");
                goto cleanUpChild;
            }

            execlp("sw", "sw", defaultWordsFileName, files->filesNamesToSearch[fileDescriptorIndex], NULL);
            fprintf(stderr, "failed to exec sw\n");
            goto cleanUpChild;
        }
        }
    }
    // We need to uninstall the default child handler
    // because its reaping loop may end just before
    // a child dies and also because we must wait for all
    // the children to end before we do csc and that is not
    // possible via only this handler and the number of children
    sigemptyset(&sigact.sa_mask);
    sigact.sa_handler = SIG_DFL;
    sigact.sa_flags = 0;
    if (sigaction(SIGCHLD, &sigact, NULL) == -1) {
        perror("There was an error setting the default SIGCHLD handler");
        goto cleanUpParent;
    }

    // Waits for all the children to end
    while (nChildProcesses > 0) {
        int status;
        if (wait(&status) == -1) {
            perror("There was an error waiting for all the sws to finish");
            goto cleanUpParent;
        }
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "Something wrong with a child\n");
                goto cleanUpParent;
            }
            --nChildProcesses;
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child killed by signal\n");
            goto cleanUpParent;
        }
    }

    bool failed = false;
    for (size_t fileIndex = 0; fileIndex < files->numberOfFiles; ++fileIndex) {
        if (close(tempFilesDescriptors[fileIndex]) == -1) {
            failed = true;
        }
    }
    if (failed) {
        perror("Failed closing one file after waiting for all the sw children");
        goto cleanUpParent;
    }

    // No need
    // Even though we waited for all the children to end the file may not have been
    // written to disk, it may still be in a buffer somewhere or in disk cache.
    // Sync makes sure that all buffered modifications to files are written to the
    // underlying filesystems.
    /*sync();*/

    int indexDescriptor;
    if (argc == 2) {
        indexDescriptor = open(defaultIndexFileName, O_WRONLY | O_TRUNC | O_CREAT | O_SYNC, 0660);
    } else {
        if (chdir(originalWd) == -1) {
            perror("Failed to change directory");
            goto cleanUpParent;
        }
        indexDescriptor = open(argv[INDEXPATHINDEX], O_WRONLY | O_EXCL | O_CREAT | O_SYNC, 0660);
    }
    if (indexDescriptor == -1) {
        perror("Csc failed to create final file for index");
        goto cleanUpParent;
    }

    pid_t pidCsc = fork();

    switch (pidCsc) {
    case -1: {
        perror("Failed to create a csc child process");
        goto cleanUpParent;
    }
    case 0: {
        // Trick csc into outputing to file instead of stdout
        if (dup2(indexDescriptor, STDOUT_FILENO) == -1) {
            perror("Shallow copy of indexDescriptor to STDOUT_FILENO failed");
            goto cleanUpChild;
        }
        execlp("csc", "csc", tempFilePath, NULL);
        fprintf(stderr, "failed to exec csc\n");
        goto cleanUpChild;
    }
    }

    // Wait for csc to end before we clean
    int status;
    if (wait(&status) == -1) {
        perror("There was an error waiting for the csc to finish");
        goto cleanUpParent;
    }

    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != EXIT_SUCCESS) {
            fprintf(stderr, "Something wrong with a child\n");
            goto cleanUpParent;
        }
    } else if (WIFSIGNALED(status)) {
        fprintf(stderr, "child killed by signal\n");
        goto cleanUpParent;
    }

    if (close(indexDescriptor) == -1) {
        perror("Failed to close csc dup file");
        goto cleanUpParent;
    }

    /*sync();*/

    // Cleansing on success
    free(originalWd);
    wipe(files);
    removeTempDir(tempFilePath);
    free(tempFilePath);
    exit(EXIT_SUCCESS);
cleanUpParent:
    free(originalWd);
    wipe(files);
    removeTempDir(tempFilePath);
    free(tempFilePath);
    exit(EXIT_FAILURE);
cleanUpChild:
    free(originalWd);
    wipe(files);
    free(tempFilePath);
    exit(EXIT_FAILURE);
}

void childHandler(__attribute__((unused)) int signo) {
    for (int i = 0; i < nChildProcesses; ++i) {
        int status;
        if (waitpid(-1, &status, WNOHANG) == -1) {
            perror("something wrong with waitpid");
            exit(EXIT_FAILURE);
        }
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != EXIT_SUCCESS) {
                fprintf(stderr, "something wrong with a child\n");
                exit(EXIT_FAILURE);
            }
            nChildProcesses--;
        } else if (WIFSIGNALED(status)) {
            fprintf(stderr, "child killed by signal\n");
            exit(EXIT_FAILURE);
        }
    }
}

void removeTempDir(char *dirPath) {
    if (dirPath == NULL) {
        return;
    }
    if (ftw(dirPath, ftwHandler, 5) == -1) {
        perror("Failed to clean files inside file temp dir");
        free(dirPath);
        exit(EXIT_FAILURE);
    }

    if (rmdir(dirPath) == -1) {
        perror("Can't delete the file temp dir");
        free(dirPath);
        exit(EXIT_FAILURE);
    }
}

int ftwHandler(const char *fpath,__attribute__((unused)) const struct stat *sb, int typeflag) {
    if (typeflag == FTW_F) {
        if (unlink(fpath) == -1) {
            perror("Failed to delete a file inside the file temp dir");
            return -1;
        }
    }

    return 0;
}

