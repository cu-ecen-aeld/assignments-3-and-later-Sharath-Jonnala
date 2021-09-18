CC = $(CROSS_COMPILE)gcc

all: default

default: writer

writer: writer.c
	$(CC) -Wall -Werror -o writer writer.c


clean:
	rm -f writer writer.o
