OBJS = $(wildcard src/*.c)

CC = clang

CFLAGS = -Wall

LDFLAGS =   `pkg-config --cflags --libs libmicrohttpd`

TARGET = exploring

all : $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)