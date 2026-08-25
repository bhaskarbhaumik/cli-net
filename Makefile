# Prefer clang (LLVM); fall back to gcc
CC      := clang
ifeq (,$(shell which $(CC) 2>/dev/null))
CC      := gcc
endif

VERSION := $(shell cat VERSION 2>/dev/null || echo 0.0.0)

CFLAGS  := -std=c11 -Wall -Wextra -O2
CPPFLAGS += -DNET_VERSION='"$(VERSION)"'
LDFLAGS := -framework SystemConfiguration \
           -framework CoreFoundation \
           -framework IOKit \
           -lncurses

TARGET  := net
SRC     := src/net.c

PREFIX  ?= /usr/local

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)

install: $(TARGET)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	install -d $(DESTDIR)$(PREFIX)/share/man/man1
	install -m 644 man/net.1 $(DESTDIR)$(PREFIX)/share/man/man1/net.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	rm -f $(DESTDIR)$(PREFIX)/share/man/man1/net.1

clean:
	rm -f $(TARGET)
	rm -rf build build-auto
