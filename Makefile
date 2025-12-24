CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11 -D_POSIX_C_SOURCE=200809L $(shell pkg-config --cflags libevdev jansson)
LDFLAGS = $(shell pkg-config --libs libevdev jansson) -lm
TARGET = keyswap

SOURCES = keyswap.c \
          key-database.c \
          config-loader.c \
          device-matcher.c \
          event-processor.c \
          debug-logger.c

OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)
