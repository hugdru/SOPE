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

#include "Files.h"

#define DIRPATHINDEX 1
#define PIPEREAD 0
#define PIPEWRITE 1

long nChildProcesses = 0;

// Name of the file which holds the words
const char defaultWordsFileName[] = "words.txt";

// "/tmp/index-pid/fileName"
// Incomplete(missing -pid) dir where we will place the sw result files
const char tempFilePathDir[] = "/tmp/index";
// Size of the buffer which holds the complete temp directory path
const size_t tempFilePathDirBufSize = (sizeof(tempFilePathDir) / sizeof(tempFilePathDir[0])) + (1 + 7 + 1) * sizeof(char);
// Size of the buffer which holds the complete temp directory path + fileName max size
const size_t tempFilePathBufSize = (sizeof(tempFilePathDir) / sizeof(tempFilePathDir[0])) + (1 + 7 + 1 + NAME_MAX) * sizeof(char);

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

    if (argc != 2) {
        fprintf(stderr, "%s <directory>\n", argv[0]);
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
    Files_t* files = getAllFilesNames(argv[DIRPATHINDEX], defaultWordsFileName);
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

    for (size_t index = 0; index < files->numberOfFiles; ++index) {

        if (nChildProcesses == maxChildProcesses) sigsuspend(&sigmask);
        ++nChildProcesses;

        pid_t pidSw = fork();

        switch (pidSw) {
            case -1:
                {
                    perror("Failed to create a sw child process");
                    goto cleanUpParent;
                }
            case 0:
                {
                    // Extend buffer to hold the temp filename, path dir + tempFilename
                    char *tempPtr = (char *) realloc(tempFilePath, tempFilePathBufSize);
                    if (tempPtr == NULL) {
                        perror("Failed to allocate space for a temp file path");
                        goto cleanUpChild;
                    }
                    tempFilePath = tempPtr;
                    snprintf(
                            &tempFilePath[tempFilePathLen], NAME_MAX,
                            "%s",
                            files->filesNamesToSearch[index]
                            );


                    int newTempFileDescriptor;
                    if ((newTempFileDescriptor = open(tempFilePath, O_WRONLY | O_CREAT | O_EXCL, 0700)) == -1) {
                        perror("There was an error creating a temporary file");
                        goto cleanUpChild;
                    }
                    if (dup2(newTempFileDescriptor, STDOUT_FILENO) == -1) {
                        perror("Failed to redirect child stdout");
                        goto cleanUpChild;
                    }

                    // Because of the chdir inside getAllFilesName(...) we don't need the files full path only the names
                    execlp("sw", "sw", defaultWordsFileName, files->filesNamesToSearch[index], NULL);
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
    do {
        if (wait(NULL) == -1) {
            if (errno == ECHILD) break;
            else {
                fprintf(stderr, "There was an error waiting for all the sws to finish\n");
                goto cleanUpParent;
            }
        }
    } while (true);
    nChildProcesses = 0;

    pid_t pidCsc = fork();

    switch (pidCsc) {
        case -1:
            {
                perror("Failed to create a csc child process");
                goto cleanUpParent;
            }
        case 0:
            {
                // Change dir to where index was called, this is needed because
                // getAllFilesNames(...) changes dir to where the search files are located
                if (chdir(originalWd) == -1) {
                    puts("Csc child failed to change directory");
                    goto cleanUpChild;
                }
                int indexDescriptor;
                if ((indexDescriptor = open("index.txt", O_WRONLY | O_EXCL | O_CREAT, 0660)) == -1) {
                    puts("Child failed to create index.txt");
                    goto cleanUpChild;
                }
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
    do {
        if (wait(NULL) == -1) {
            if (errno == ECHILD) break;
            else {
                fprintf(stderr, "There was an error waiting for the csc to finish\n");
                goto cleanUpParent;
            }
        }
    } while (true);

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
            nChildProcesses--;
        } else if (WIFSIGNALED(status)) {
            perror("child killed by signal");
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

