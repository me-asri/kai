CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Werror -O2 -g

TARGET  = kai
SOURCES = $(wildcard *.c)
DEPS    = $(wildcard *.h)
OBJ     = $(SOURCES:.c=.o)

.PHONY: clean all

all: $(TARGET)

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -f $(TARGET) *.o
