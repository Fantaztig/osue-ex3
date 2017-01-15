#
# Author: David Schröder 1226747
# Description: All compiles server and client for the exercise
#				in operating systems at TU Wien, client and 
#				server may be compiled separately
#
CC = gcc
DEFS = -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS = -std=c99 -pedantic -Wall -g -pthread $(DEFS)

OBJECTFILES = server.o client.o

.PHONY: all clean

all: auth-server auth-client

auth-client: client.o
	$(CC) $(CFLAGS) -o $@ $^ -lrt

auth-server: server.o
	$(CC) $(CFLAGS) -o $@ $^ -lrt

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

server.o: server.c server.h

client.o: client.c client.h

clean:
	rm -f $(OBJECTFILES) auth-client auth-server