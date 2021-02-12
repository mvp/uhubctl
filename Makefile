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
PKG_CONFIG	?= pkg-config

CC ?= gcc
CFLAGS ?= -g -O0
CFLAGS += -Wall -Wextra -std=c99 -pedantic
GIT_VERSION := $(shell git describe --match "v[0-9]*" --abbrev=8 --dirty --tags | cut -c2-)
ifeq ($(GIT_VERSION),)
	GIT_VERSION := $(shell cat VERSION)
endif
CFLAGS += -DPROGRAM_VERSION=\"$(GIT_VERSION)\"

# Use hardening options on Linux
ifeq ($(UNAME_S),Linux)
	LDFLAGS += -Wl,-zrelro,-znow
endif

# Use pkg-config if available
ifneq (,$(shell which $(PKG_CONFIG)))
	CFLAGS  += $(shell $(PKG_CONFIG) --cflags libusb-1.0)
	LDFLAGS += $(shell $(PKG_CONFIG) --libs libusb-1.0)
else
# But it should still build if pkg-config is not available (e.g. Linux or Mac homebrew)
	LDFLAGS += -lusb-1.0
endif

PROGRAM = uhubctl

$(PROGRAM): $(PROGRAM).c
	$(CC) $(CPPFLAGS) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

install:
	$(INSTALL_DIR) $(DESTDIR)$(sbindir)
	$(INSTALL_PROGRAM) $(PROGRAM) $(DESTDIR)$(sbindir)

clean:
	$(RM) $(PROGRAM).o $(PROGRAM).dSYM $(PROGRAM)
