SRCS=./shell/main.c ./shell/shell.c ./io/io.c ./parser/parser.c
EXECUTABLES=./build/tinyshell

CC=gcc
CFLAGS=-g -Wall 
LDFLAGS=

OBJS=$(SRCS:.c=.o)

all: $(EXECUTABLES)

$(EXECUTABLES): $(OBJS)
	mkdir build
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@  

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(EXECUTABLES) $(OBJS)
	rmdir build
