CC := gcc
BUILD_ARGS := -g -O2 \
	-Werror -Wall
TEST_BUILD_ARGS := -ggdb \
	-Werror -Wall -fsanitize=address
VALGRIND_CMD := valgrind -s --track-origins=yes --leak-check=yes --leak-check=full --show-leak-kinds=all
SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))
DEPENDS := $(patsubst %.c,%.d,$(SOURCES))

.PHONY: clean

ifeq ($(USE_CUSTOM_ALLOC),yes)
main.out: $(OBJECTS)
	$(CC) $(BUILD_ARGS) main.o server.o MurmurHash3.o -o main.out -lalloc
else
main.out: $(OBJECTS)
	$(CC) $(BUILD_ARGS) main.o server.o MurmurHash3.o -o main.out
endif

debug:
	$(CC) $(TEST_BUILD_ARGS) main.o server.o MurmurHash3.o -o main.out

# Recompile when headers change
# - is used to ignore if some dependencies are not found
-include $(DEPENDS)

%.o: %.c
	$(CC) $(BUILD_ARGS) -fPIC -MMD -MP -c '$<' -o '$@'

memcheck:
	$(CC) -g -O2 -Werror -Wall main.c server.c MurmurHash3.c -o main.o -lalloc
	$(VALGRIND_CMD) ./main.o 127.0.0.1 8007

client: client.o
	${CC} ${BUILD_ARGS} client.o -o client.out

client_memcheck:
	$(CC) -g -O2 -Werror -Wall client.c -o client.o
	$(VALGRIND_CMD) ./client.o 127.0.0.1 8007

clean:
	rm -f $(OBJECTS) $(DEPENDS) *.gch *.out
