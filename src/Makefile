CC=cc
CFLAGS=-O3

TARGETS=bfc bfpp bfcc

all: $(TARGETS)

bfc: bfc.c
		$(CC) $(CFLAGS) $< -o $@

bfpp: bfpp.c
		$(CC) $(CFLAGS) $< -o $@

bfcc: bfcc.c
		$(CC) $(CFLAGS) $< -o $@

install: $(TARGETS)
		install $(TARGETS) /usr/local/bin

uninstall:
		rm -f /usr/local/bin/bfc /usr/local/bin/bfpp /usr/local/bin/bfcc

clean:
		rm -f $(TARGETS)
