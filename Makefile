.PHONY: test

OPT ?= -O3

CC = gcc
CFLAGS = -I/usr/local/include -Iinclude -fPIC -Wall -Wextra -lc -lm -std=gnu99 -g $(OPT)
LDFLAGS = -shared

TARGET = hdrhistogram.so
SOURCES = $(wildcard src/*.c)
HEADERS = $(wildcard include/*.h)
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) -o $@ $(LDFLAGS) -lm -lc $(OBJECTS) -lhdr_histogram

noopt:
	$(MAKE) OPT="-O0"

clean:
	find . -type f -name '*.o' -delete
	find . -type f -name '*.so' -delete
	find . -type f -name '*.pyc' -delete

test: $(TARGET)
	py.test -v test
