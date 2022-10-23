# Makefile for dirdiff
#
# In fact all we have to make is the libfilecmp.so.0.0 library.

BINDIR=$(DESTDIR)/usr/bin
LIBDIR=$(DESTDIR)/usr/lib
INSTALL=install

# You may need to change the -I arguments depending on your system
CFLAGS=-O3 $(pkg-config --cflags tcl)
LIBS = $(pkg-config --libs tcl)
SONAME = libfilecmp.so.0.0

all:	$(SONAME)

$(SONAME): filecmp.c
	$(CC) $(CFLAGS) $(LDFLAGS) -shared -Wl,-soname=$(SONAME) -fPIC -o $@ filecmp.c $(LIBS)

install: dirdiff $(SONAME)
	$(INSTALL) -c dirdiff $(BINDIR)
	$(INSTALL) -c $(SONAME) $(LIBDIR)

clean:
	rm -f $(SONAME)
