NAME = zlib
VERSION = 1.2.3
PALM_VERSION = 1.4.0

SRC_BZ2 = http://palm.cdnetworks.net/opensource/${PALM_VERSION}/${NAME}-${VERSION}.tar.bz2
PATCH = http://palm.cdnetworks.net/opensource/${PALM_VERSION}/${NAME}-${VERSION}-patches.tgz

include ../../support/download.mk

.PHONY: unpack
unpack: build/.unpacked

build/.unpacked: ${DL_DIR}/${NAME}-${VERSION}.tar.bz2
	rm -rf build
	mkdir -p build
	tar -C build -j -x -f $<
	mv build/${NAME}-${VERSION} build/src
	${MAKE} SRC_FILE=${PATCH} \
		LOCAL_FILE=${NAME}-${VERSION}-patches.tgz download
	tar -x -f ${DL_DIR}/${NAME}-${VERSION}-patches.tgz -O \
		zlib-patches/visibility.patch zlib-patches/autotools.patch | patch -d build/src -p1
	touch $@

include ../../support/staging.mk

include ../../support/stage-rootfs.mk

stage:: build/armv7.built

build/%.built: build/.unpacked
	rm -f $@
	install -d ${STAGING_DIR}/usr/include
	install -m 644 build/src/zlib.h ${STAGING_DIR}/usr/include/
	install -m 644 build/src/zconf.h ${STAGING_DIR}/usr/include/
	touch $@

clobber::
	rm -f ${STAGING_DIR}/usr/include/zlib.h
	rm -f ${STAGING_DIR}/usr/include/zconf.h
	rm -rf build
