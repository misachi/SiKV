all:
	gcc -g -fsanitize=address -Werror -Wall main.c MurmurHash3.c -o main.o

memcheck:
	valgrind -s --track-origins=yes --leak-check=yes ./main.o

clean:
	rm main.o