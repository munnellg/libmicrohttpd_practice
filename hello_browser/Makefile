OBJS = $(wildcard src/*.c)

CC = clang

CFLAGS = -Wall

LDFLAGS =   `pkg-config --cflags --libs libmicrohttpd`

TARGET = hello

all : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)