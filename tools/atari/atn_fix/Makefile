TARGET = atn_fix.ttp

CROSS = yes

ifneq (,$(filter $(CROSS),Y yes))
        CROSSPREFIX=m68k-atari-mintelf-
endif

PREFIX=$(shell $(CROSSPREFIX)gcc -print-sysroot)/opt
ifeq ($(PREFIX),)
  PREFIX=/opt
endif

LIBCMINI_INCLUDE = $(PREFIX)/libcmini/include
LIBCMINI_LIB     = $(PREFIX)/libcmini/lib
LIBCMINI_STARTUP = $(LIBCMINI_LIB)

CC      = $(CROSSPREFIX)gcc
CFLAGS  = -O2 -fomit-frame-pointer -I$(LIBCMINI_INCLUDE) -Wall -DAPP_NAME=\"$(TARGET)\"
LDFLAGS = -L$(LIBCMINI_LIB) -lcmini -lgcc

default: $(TARGET)

$(TARGET): atn_fix.c disk_struct.h byte_swap.h
	$(CC) -s -nostdlib $(LIBCMINI_STARTUP)/crt0.o $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(TARGET) *~
