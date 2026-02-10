CC = gcc

STD = -std=c99

CFLAGS = -Wall -Wextra $(STD) 


SRC = $(wildcard src/*.c)

TARGET = live-viewer

all: $(TARGET)

$(TARGET):
	$(CC) $(SRC) $(CFLAGS) -o $(TARGET)

run:
	./$(TARGET)

clean:
	rm -f $(TARGET)
