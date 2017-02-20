# uhubctl Makefile
#
UNAME_S := $(shell uname -s)

DESTDIR ?=
prefix  ?= /usr
sbindir ?= $(prefix)/sbin

INSTALL		:= install
INSTALL_DIR	:= $(INSTALL) -m 755 -d
INSTALL_PROGRAM	:= $(INSTALL) -m 755
RM		:= rm -rf

CC ?= gcc
CFLAGS ?= -g -O0

CFLAGS	+= -Wall -Wextra

ifeq ($(UNAME_S),Linux)
	LDFLAGS	+= -Wl,-z,relro
endif

PROGRAM = uhubctl

$(PROGRAM): $(PROGRAM).c
	$(CC) $(CFLAGS) $@.c -o $@ -lusb-1.0 $(LDFLAGS)

install:
	$(INSTALL_DIR) $(DESTDIR)$(sbindir)
	$(INSTALL_PROGRAM) $(PROGRAM) $(DESTDIR)$(sbindir)

clean:
	$(RM) $(PROGRAM).o $(PROGRAM).dSYM $(PROGRAM)
