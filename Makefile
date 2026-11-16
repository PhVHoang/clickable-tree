CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c11
PREFIX ?= /usr/local

all: ctree

ctree: ctree.c
	$(CC) $(CFLAGS) -D_XOPEN_SOURCE=700 -o $@ $<

test: ctree
	sh tests/test.sh

install: ctree
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 ctree $(DESTDIR)$(PREFIX)/bin/ctree

clean:
	rm -f ctree

.PHONY: all test install clean
