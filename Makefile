CC=gcc
CFLAGS=-Wall -Wextra -Isrc

SRC_DIR=src
OBJS=server.o net.o file.o mime.o cache.o hashtable.o llist.o

all: server

server: $(OBJS)
	gcc -o $@ $^

%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

net.o: $(SRC_DIR)/net.c $(SRC_DIR)/net.h

server.o: $(SRC_DIR)/server.c $(SRC_DIR)/net.h

file.o: $(SRC_DIR)/file.c $(SRC_DIR)/file.h

mime.o: $(SRC_DIR)/mime.c $(SRC_DIR)/mime.h

cache.o: $(SRC_DIR)/cache.c $(SRC_DIR)/cache.h

hashtable.o: $(SRC_DIR)/hashtable.c $(SRC_DIR)/hashtable.h

llist.o: $(SRC_DIR)/llist.c $(SRC_DIR)/llist.h

clean:
	rm -f $(OBJS)
	rm -f server

.PHONY: all clean
