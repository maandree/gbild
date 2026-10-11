.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)

OBJ =\
	dragging.o\
	dropping.o\
	events.o\
	files.o\
	filters.o\
	image.o\
	opening.o\
	printing.o\
	properties.o\
	transform.o\
	ui.o\
	zoom.o\
	logical-scroll-pane.o\
	gbild.o

HDR =\
	common.h\
	logical-scroll-pane.h

all: gbild
$(OBJ): $(HDR)

.c.o:
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

gbild: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

install: gbild
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man1/"
	mkdir -p -- "$(DESTDIR)$(PREFIX)/share/applications/"
	cp -- gbild "$(DESTDIR)$(PREFIX)/bin/"
	cp -- gbild.1 "$(DESTDIR)$(MANPREFIX)/man1/"
	cp -- gbild.desktop "$(DESTDIR)$(PREFIX)/share/applications/"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/bin/gbild"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man1/gbild.1"
	-rm -f -- "$(DESTDIR)$(PREFIX)/share/applications/gbild.desktop"

clean:
	-rm -f -- *.o *.a *.lo *.su *.so *.so.* *.gch *.gcov *.gcno *.gcda
	-rm -f -- gbild

.SUFFIXES:
.SUFFIXES: .o .c

.PHONY: all install uninstall clean
