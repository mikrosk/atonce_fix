TARGET = atn_fix.ttp

CROSS = yes

ifneq (,$(filter $(CROSS),Y yes))
        CROSSPREFIX=m68k-atari-mint-
endif

PREFIX=$(shell $(CROSSPREFIX)gcc -print-sysroot)/usr
ifeq ($(PREFIX),)
  PREFIX=/usr
endif

LIBCMINI_INCLUDE = $(PREFIX)/include/libcmini
LIBCMINI_LIB     = $(PREFIX)/lib/libcmini
LIBCMINI_STARTUP = $(LIBCMINI_LIB)

CC      = $(CROSSPREFIX)gcc
CFLAGS  = -O2 -fomit-frame-pointer -I$(LIBCMINI_INCLUDE) -Wall -DAPP_NAME=\"$(TARGET)\"
LDFLAGS = -L$(LIBCMINI_LIB) -lcmini -lgcc

default: $(TARGET)

$(TARGET): at_disk.c disk_struct.h byte_swap.h
	$(CC) -s -nostdlib $(LIBCMINI_STARTUP)/crt0.o $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET) *~
