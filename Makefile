CC := gcc
BUILD_ARGS := -g -O2 \
	-fsanitize=address \
	-Werror -Wall
VALGRIND_CMD := valgrind -s --track-origins=yes --leak-check=yes --leak-check=full --show-leak-kinds=all

all:
	${CC} ${BUILD_ARGS} main.c server.c MurmurHash3.c -o main.o

memcheck:
	${CC} -g -O2 -Werror -Wall main.c server.c MurmurHash3.c -o main.o
	${VALGRIND_CMD} ./main.o

client:
	${CC} ${BUILD_ARGS} client.c -o client.o

client_memcheck:
	${CC} -g -O2 -Werror -Wall client.c -o client.o
	${VALGRIND_CMD} ./client.o 127.0.0.1 8007

clean:
	rm -f main.o client.o server.o
