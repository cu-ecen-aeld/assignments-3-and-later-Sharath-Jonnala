ifeq ($(CC),)
	CC = ${CROSS_COMPILE}gcc
endif

ifeq ($(CFLAGS),)
	CFLAGS = -g -Wall -Werror 
endif

ifeq ($(LDFLAGS),)
	LDFLAGS = -pthread -lrt
endif

all: aesdsocket

default: aesdsocket

aesdsocket: aesdsocket.o
	 $(CC) $(CFLAGS) -o aesdsocket aesdsocket.o $(LDFLAGS)
aesdsocket.o: aesdsocket.c
	 $(CC) $(CFLAGS) -c aesdsocket.c queue.h

clean:
	rm -f aesdsocket *.o
