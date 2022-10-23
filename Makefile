# Makefile for dirdiff
#
# In fact all we have to make is the libfilecmp.so.0.0 library.

BINDIR=$(DESTDIR)/usr/bin
LIBDIR=$(DESTDIR)/usr/lib
INSTALL=install

# You may need to change the -I arguments depending on your system
CFLAGS=-O3 -I/usr/include/tcl8.3/ -I/usr/include/tcl

all:	libfilecmp.so.0.0

libfilecmp.so.0.0: filecmp.c
	$(CC) $(CFLAGS) -shared -o $@ filecmp.c

install: dirdiff libfilecmp.so.0.0
	$(INSTALL) -c dirdiff $(BINDIR)
	$(INSTALL) -c libfilecmp.so.0.0 $(LIBDIR)

clean:
	rm -f libfilecmp.so.0.0
