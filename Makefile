# Prefer clang (LLVM); fall back to gcc
CC      := clang
ifeq (,$(shell which $(CC) 2>/dev/null))
CC      := gcc
endif

CFLAGS  := -std=c11 -Wall -Wextra -O2
LDFLAGS := -framework SystemConfiguration \
           -framework CoreFoundation \
           -framework IOKit \
           -lncurses

TARGET  := net
SRC     := src/net.c

PREFIX  ?= /usr/local

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

clean:
	rm -f $(TARGET)
