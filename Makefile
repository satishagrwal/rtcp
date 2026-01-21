CC = gcc
CFLAGS = -Wall -Wextra
TARGET = sdp_bw

all: build

build:
	$(CC) $(CFLAGS) sdp_bw.c -o $(TARGET)

test: build
	sh tests/run_tests.sh

clean:
	rm -f $(TARGET) out.txt err.txt
