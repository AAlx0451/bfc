CC=cc
CFLAGS=-O3

TARGETS=bfc bfpp

all: $(TARGETS)

bfc: bfc.c
		$(CC) $(CFLAGS) $< -o $@

bfpp: bfpp.c
		$(CC) $(CFLAGS) $< -o $@

install: $(TARGETS)
		install $(TARGETS) /usr/local/bin

uninstall:
		rm -f /usr/local/bin/bfc /usr/local/bin/bfpp

clean:
		rm -f $(TARGETS)
