# source files.

SRC = $(wildcard src/*.c)

OBJ = $(patsubst src/%.c, buildtemp/%.o, $(SRC))

OUT = ../../bin/sw

# compiler
CC = gcc

.SUFFIXES: .c

all: default

release: CFLAGS = -std=c11 -O2 -march=native -pipe -fstack-protector-all -Wstack-protector --param ssp-buffer-size=4 -pie -fPIE -ftrapv -D_FORTIFY_SOURCE=2 -Wl,-z,relro,-z,now
release: $(OUT)

#default: CFLAGS = -std=c11 -O2 -march=native -pipe
default: debug
default: $(OUT)

debug: CFLAGS = -std=c11 -ggdb -g3 -Wall -Wextra -pedantic -Wshadow -Wfloat-equal -Wcast-align -Wcast-qual -Wwrite-strings -Wconversion -Wsign-conversion -Wlogical-op -Wmissing-declarations -Wredundant-decls -Wdisabled-optimization -Winline -Wswitch-default -Wswitch-enum -Wnested-externs -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wformat=2 -Wpadded -Wimplicit -Wmissing-include-dirs -Wbad-function-cast -Wc++-compat -Wcast-qual -Wcast-align -Wpacked -Wvector-operation-performance -Wdouble-promotion
debug: $(OUT)

buildtemp/%.o: src/%.c
	mkdir -p buildtemp
	$(CC) $(CFLAGS) -c $< -o $@

$(OUT): $(OBJ)
	mkdir -p ../../bin
	$(CC) $(CFLAGS) $(OBJ) -o $(OUT)

clean:
	rm -f $(OBJ) $(OUT)

test:
	echo $(SRC)
	echo $(OBJ)
