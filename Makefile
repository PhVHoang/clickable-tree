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
	install -d $(DESTDIR)$(PREFIX)/share/ctree
	install -m 644 shell/ctree.bash $(DESTDIR)$(PREFIX)/share/ctree/ctree.bash
	install -m 644 shell/ctree.zsh $(DESTDIR)$(PREFIX)/share/ctree/ctree.zsh
	install -m 644 shell/ctree.fish $(DESTDIR)$(PREFIX)/share/ctree/ctree.fish

clean:
	rm -f ctree

.PHONY: all test install clean
