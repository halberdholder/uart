
TARGET = demo

SRC_DIR = .
OBJ_DIR = .

SRC = $(wildcard ${SRC_DIR}/*.c)
OBJ = $(patsubst %.c, ${OBJ_DIR}/%.o, $(notdir ${SRC}))

CC = gcc
CFLAGS = -Wall

all: ${TARGET}

${TARGET}: ${OBJ}
	$(CC) ${OBJ} -o ${TARGET}

${OBJ}: ${SRC}
	${CC} ${CFLAGS} -c $< -o $@

clean:
	rm -rf *.o ${TARGET}

.PHONY: all clean
