NAME = fuse
VERSION = 2.7.2
DEPENDS = 

SRC_GZ = http://palm.cdnetworks.net/opensource/1.3.5/fuse-2.7.2.tar.gz
PATCH =  http://palm.cdnetworks.net/opensource/1.3.5/fuse-2.7.2-patches.tgz

.PHONY: unpack
unpack: build/.unpacked

build/.unpacked: 
	rm -rf build
	mkdir -p build
	curl ${SRC_GZ} | tar -C build -zxf -
	mv build/${NAME}-${VERSION} build/src
	curl ${PATCH} | zcat | patch -d build/src -p1
	touch $@

include ../../../support/staging.mk

include ../../../support/stage-rootfs.mk

include ../../../support/cross-compile.mk

stage:: build/armv7.built

build/%.built: build/.unpacked
	rm -f $@
	( cd build/src ; ${SB2}	./configure --prefix=${INSTALL_PREFIX} --disable-kernel-module --disable-util )
	( cd build/src ; ${SB2} make )
	( cd build/src ; ${SB2} make install-data )
	touch $@

clobber::
	rm -rf build