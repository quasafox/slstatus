# slstatus version
VERSION = 1.0

# customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

# flags
CPPFLAGS = -D_DEFAULT_SOURCE -D_GNU_SOURCE -DVERSION=\"${VERSION}\"
CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter -O2 -flto
LDFLAGS  = -s
# OpenBSD: add -lsndio
# FreeBSD: add -lkvm -lsndio
LDLIBS   =

# compiler and linker
CC = cc
