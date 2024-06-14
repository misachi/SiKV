all:
	gcc -g -fsanitize=address -Werror -Wall main.c server.c MurmurHash3.c -o main.o

memcheck:
	gcc -g -Werror -Wall main.c server.c MurmurHash3.c -o main.o
	valgrind -s --track-origins=yes --leak-check=yes --leak-check=full --show-leak-kinds=all ./main.o

client:
	gcc -g -fsanitize=address -Werror -Wall client.c -o client.o

client_memcheck:
	gcc -g -Werror -Wall client.c -o client.o
	valgrind -s --track-origins=yes --leak-check=yes --leak-check=full --show-leak-kinds=all ./client.o 127.0.0.1 8007

clean:
	rm main.o