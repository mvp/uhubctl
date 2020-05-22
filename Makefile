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
CFLAGS += -Wall -Wextra -std=c99 -pedantic
GIT_VERSION := $(shell git describe --match "v[0-9]*" --abbrev=8 --dirty --always --tags | cut -c2-)
ifeq ($(GIT_VERSION),)
    GIT_VERSION := $(shell cat VERSION)
endif
CFLAGS += -DPROGRAM_VERSION=\"$(GIT_VERSION)\"

ifeq ($(UNAME_S),Linux)
	LDFLAGS += -Wl,-zrelro,-znow -lusb-1.0
endif

ifeq ($(UNAME_S),Darwin)
ifneq ($(wildcard /opt/local/include),)
	# MacPorts
	CFLAGS  += -I/opt/local/include
	LDFLAGS += -L/opt/local/lib
endif
	LDFLAGS += -lusb-1.0
endif

ifeq ($(UNAME_S),FreeBSD)
	LDFLAGS += -lusb
endif

ifeq ($(UNAME_S),NetBSD)
    CFLAGS  += $(shell pkg-config --cflags libusb-1.0)
    LDFLAGS += $(shell pkg-config --libs libusb-1.0)
endif

PROGRAM = uhubctl

$(PROGRAM): $(PROGRAM).c
	$(CC) $(CPPFLAGS) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

install:
	$(INSTALL_DIR) $(DESTDIR)$(sbindir)
	$(INSTALL_PROGRAM) $(PROGRAM) $(DESTDIR)$(sbindir)

clean:
	$(RM) $(PROGRAM).o $(PROGRAM).dSYM $(PROGRAM)
