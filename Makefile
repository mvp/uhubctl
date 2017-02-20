# uhubctl Makefile
#
DESTDIR ?=
prefix  ?= /usr
sbindir ?= $(prefix)/sbin

INSTALL		:= install
INSTALL_DIR	:= $(INSTALL) -m 755 -d
INSTALL_PROGRAM	:= $(INSTALL) -m 755
RM		:= rm -f

CC ?= gcc
CFLAGS ?= -g -O0

CFLAGS	+= -Wall -Wextra
LDFLAGS	+= -Wl,-z,relro

PROGRAM = uhubctl

$(PROGRAM): $(PROGRAM).c
	$(CC) $(CFLAGS) $@.c -o $@ -lusb-1.0 $(LDFLAGS)

install:
	$(INSTALL_DIR) $(DESTDIR)$(sbindir)
	$(INSTALL_PROGRAM) $(PROGRAM) $(DESTDIR)$(sbindir)

clean:
	$(RM) $(PROGRAM).o $(PROGRAM).dSYM $(PROGRAM)
