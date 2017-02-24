# uhubctl Makefile
#
VERSION = 1
PATCHLEVEL = 6
EXTRAVERSION = -dev
UHUBCTLVERSION = $(VERSION)$(if $(PATCHLEVEL),.$(PATCHLEVEL))$(EXTRAVERSION)

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

CFLAGS += -std=c99 -Wall -Wextra -pedantic
CFLAGS += -D'PROGRAM_VERSION="$(UHUBCTLVERSION)"'

# Remove the following if your system doesn't support IEEE Standard 1003.1b-1993
# a.k.a 1993 edition of the POSIX.1b standard
CFLAGS += -D'_POSIX_C_SOURCE=199309L'

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
