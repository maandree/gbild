PREFIX    = /usr
MANPREFIX = $(PREFIX)/share/man

CC = c99

CPPFLAGS = -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_XOPEN_SOURCE=700 -D_GNU_SOURCE
CFLAGS   = $$(pkg-config --cflags gtk+-2.0 gexiv2)
LDFLAGS  = $$(pkg-config --libs gtk+-2.0 gexiv2) -lsimple -lm
