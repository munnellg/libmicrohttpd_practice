OBJS = $(wildcard src/*.c)

CC = clang

CFLAGS = -Wall

LDFLAGS = `pkg-config --cflags --libs libmicrohttpd`

TARGET = response

all : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)