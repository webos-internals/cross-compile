NAME = xcb-proto
VERSION = 1.6

DEPENDS = python/python

SRC_BZ2 = http://xcb.freedesktop.org/dist/${NAME}-${VERSION}.tar.bz2

include ../../../support/download.mk

.PHONY: unpack
unpack: build/.unpacked

build/.unpacked: ${DL_DIR}/${NAME}-${VERSION}.tar.bz2
	rm -rf build
	mkdir -p build
	tar -C build -j -x -f $<
	mv build/${NAME}-${VERSION} build/src
	touch $@

include ../../../support/staging.mk

stage:: build

include ../../../support/cross-compile.mk

build: build/armv7.built

build/%.built: build/.unpacked
	rm -f $@
	( cd build/src ; ${SB2} \
	./configure \
		--prefix=${INSTALL_PREFIX} \
	)
	( cd build/src ; ${SB2} make)
	( cd build/src ; ${SB2} make install )
	touch $@

clobber::
	rm -rf build