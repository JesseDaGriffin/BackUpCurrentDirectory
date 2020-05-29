CC = gcc
CFLAGS = -Wall -Werror -pthread
TARGET = BackItUp
HEADERS = helpers.h

.PHONY: clean

all: backitup

${TARGET}.o: ${TARGET}.c ${HEADERS}
	${CC} -c ${TARGET}.c

backitup: ${TARGET}.o
	${CC} -o backitup ${TARGET}.o ${CFLAGS}

run: all
	./backitup

debug:
	${CC} -o backitup-debug ${TARGET}.c -g ${CFLAGS}

clean:
	rm -rf *.o backitup backitup-debug *.dSYM/
