Compilation instructions:
    In the code directory there is a makefile which calls
    each of the 2 programs makefiles

    code/makefile--------------|
        code/balcao/makefile---|
        code/ger_cl/makefile---|

    make (defaults to debug)
    make all (same as make)
    make debug (lots of warnings turned on and debugging support)
    make release (optimization and exploit protection)

    The executables are placed in the bin folder

    the object files are placed in
        code/balcao/buildtemp
        code/ger_cl/buildtemp

Execution instructions:

    Temporary folder path /tmp/sope, it gets deleted when program
    ends because of error or not.
