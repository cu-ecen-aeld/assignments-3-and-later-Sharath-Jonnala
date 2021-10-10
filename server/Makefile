CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror
LDFLAGS = -pthread -lrt

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.c
	$(CC) $^ -o $@ $(INCLUDES) $(LDFLAGS) $(CFLAGS)

clean:
	rm -f aesdsocket *.o
