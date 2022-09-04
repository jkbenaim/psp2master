LIBCDIO_VERSION = 2.1.0
LIBCDIO_NAME = libcdio-$(LIBCDIO_VERSION)
PSPDECRYPT_VERSION = 1.0
PSPDECRYPT_NAME = pspdecrypt-$(PSPDECRYPT_VERSION)
HOST = i686-w64-mingw32
CC = ${HOST}-gcc
CXX = ${HOST}-g++
LD = ${HOST}-ld

target  ?= psp2master
objects := $(patsubst %.c,%.o,$(wildcard *.c)) data.o

#libs:=libiso9660
libs := openssl

#EXTRAS += -fsanitize=undefined -fsanitize=null -fcf-protection=full -fstack-protector-all -fstack-check -Wimplicit-fallthrough -Wall -flto -ffat-lto-objects
#EXTRAS += -Wimplicit-fallthrough -Wall -fanalyzer
EXTRAS += -static

ifdef libs
LDLIBS  += $(shell mingw32-pkg-config --libs   ${libs})
CFLAGS  += $(shell mingw32-pkg-config --cflags ${libs})
endif

LDLIBS += -lws2_32
LDFLAGS += ${EXTRAS}
CFLAGS  += -std=gnu2x -Og -ggdb -Ilibcdio-install/include ${EXTRAS}

.PHONY: all
all:	$(target)

.PHONY: clean
clean:
	rm -f $(target) $(target).exe $(objects)

.PHONY: distclean
distclean: clean
	rm -Rf libcdio-build libcdio-install $(LIBCDIO_NAME) $(PSPDECRYPT_NAME) lib/*

data.o: data/cont_l0.img data/mdi.img data/umd_auth.dat
	$(LD) -r -b binary data/* -o $@

$(target): $(objects) lib/pspdecrypt.a libcdio-install/lib/libiso9660.a libcdio-install/lib/libcdio.a data.o



########################
########################
# libcdio stuff
########################
########################


$(objects): libcdio-install/lib/libcdio.a libcdio-install/lib/libiso9660.a


libcdio-install/lib/libcdio.a libcdio-install/lib/libiso9660.a: libcdio-build/Makefile
	$(MAKE) -C libcdio-build install MAKEFLAGS=

libcdio-build/Makefile: $(LIBCDIO_NAME)/configure
	mkdir -p libcdio-build
	cd libcdio-build && ../$(LIBCDIO_NAME)/configure --disable-shared --enable-static --without-cd-drive --without-cd-info --without-cdda-player --without-cd-read --without-iso-info --without-iso-read --disable-cxx --disable-joliet --disable-rock --disable-cddb --disable-vcd-info --disable-example-progs --disable-cpp-progs --prefix="`realpath ../libcdio-install`" --host=${HOST}

$(LIBCDIO_NAME)/configure: libraries/$(LIBCDIO_NAME).tar.gz
	tar -zxvf $<
	touch $@


########################
########################
# pspdecrypt stuff
########################
########################


lib/pspdecrypt.a: $(PSPDECRYPT_NAME)/pspdecrypt.a
	cp -v $< $@

$(PSPDECRYPT_NAME)/pspdecrypt.a: $(PSPDECRYPT_NAME)/Makefile
	$(MAKE) -C $(PSPDECRYPT_NAME)

$(PSPDECRYPT_NAME)/Makefile: libraries/$(PSPDECRYPT_NAME).tar.gz
	tar -zxvf $<
	patch -d $(PSPDECRYPT_NAME) -p1 -i ../libraries/pspdecrypt-make-library.diff
