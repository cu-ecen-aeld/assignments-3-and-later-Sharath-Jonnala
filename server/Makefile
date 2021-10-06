CC = $(CROSS_COMPILE)gcc

all: socket

socket: aesdsocket.c
	$(CC) -Wall -Werror -c -o aesdsocket.o aesdsocket.c
	$(CC) -Wall -Werror -o aesdsocket aesdsocket.o
clean:
	rm -f aesdsocket aesdsocket.o
