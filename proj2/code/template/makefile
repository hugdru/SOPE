# source files.

SRC = $(wildcard src/*.c)

OBJ = $(patsubst src/%.c, buildtemp/%.o, $(SRC))

OUT = ../../bin/sw

# compiler
CC = colorgcc -std=c11
LIBS = -pthread -lrt


# Debug flags
DEBUG_WARNINGS_FLAGS = -Wall -Wextra -pedantic -Wshadow -Wfloat-equal -Wcast-align -Wcast-qual -Wwrite-strings -Wconversion -Wsign-conversion -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wdisabled-optimization -Winline -Wswitch-default -Wswitch-enum -Wnested-externs -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wformat=2 -Wpadded -Wimplicit -Wmissing-include-dirs -Wbad-function-cast -Wc++-compat -Wcast-qual -Wcast-align -Wpacked -Wvector-operation-performance -Wdouble-promotion

# Release flags
RELEASE_OPTIMIZATION_FLAGS = -O2 -march=native -pipe
RELEASE_PROTECTION_FLAGS = -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -pie -fPIE -ftrapv -D_FORTIFY_SOURCE=2 -Wl,-z,relro,-z,now

.SUFFIXES: .c

all: default

release: CFLAGS = $(RELEASE_OPTIMIZATION_FLAGS)
release: $(OUT)

default: debug
default: $(OUT)

debug: CFLAGS = -ggdb -g3 $(DEBUG_WARNINGS_FLAGS)
debug: $(OUT)

buildtemp/%.o: src/%.c
	mkdir -p buildtemp
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	mkdir -p ../../bin
	$(CC) $(LIBS) $(CFLAGS) $(OBJ) -o $(OUT)

clean:
	rm -f $(OBJ) $(OUT)

test:
	echo $(SRC)
	echo $(OBJ)
