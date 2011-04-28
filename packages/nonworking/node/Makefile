NAME = node
VERSION = 0.3.2
DEPENDS = 

SRC_TGZ = http://nodejs.org/dist/${NAME}-v${VERSION}.tar.gz

include ../../../support/download.mk

.PHONY: unpack
unpack: build/.unpacked

build/.unpacked: ${DL_DIR}/${NAME}-${VERSION}.tar.gz
	rm -rf build
	mkdir -p build
	tar -C build -x -f $<
	mv build/${NAME}-v${VERSION} build/src
	touch $@

include ../../../support/staging.mk

include ../../../support/cross-compile.mk

stage-local:: build/${ARCH}.built

build/%.built: build/.unpacked
	rm -f $@
	mkdir -p build/$*
	cp -R build/src/* build/$*
	( cd build/$* ; ${SB2} ./configure )
	( cd build/$* ; ${SB2} $(MAKE) && ${SB2} make install )
	touch $@

clobber::
	rm -rf build