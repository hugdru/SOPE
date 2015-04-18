Compilation instructions:
    In the code directory there is a makefile which calls
    each of the 3 programs makefiles, repectively: index,
    csc and sw.

    code/makefile-------------|
        code/index/makefile---|
        code/csc/makefile-----|
        code/sw/makefile------|

    make (defaults to debug)
    make all (same as make)
    make debug (lots of warnings turned on and debugging support)
    make release (optimization and exploit protection)

    The executables are placed in the folder bin

    the object files are placed in
        code/index/buildtemp
        code/csc/buildtemp
        code/sw/buildtemp

Execution instructions:
    Add bin to the path, if in the root project folder
    export PATH="$PATH:$(pwd)/bin"

    Temporary folder path /tmp/index-{pid}, it gets deleted when program
    ends because of error or not.
