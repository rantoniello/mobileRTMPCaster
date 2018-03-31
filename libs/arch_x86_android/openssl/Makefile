include ../../config.common.mk
include ../../config.android-x86.mk

LIBDIR := "$(INSTALL_DIR)/lib"

_3RDPLIB := openssl-1.0.1h

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR}
CONFIGURE_PARAMS+= shared

all: install

config:
	mkdir -p $(INSTALL_DIR)/include
	if [ ! -f $(_3RDPLIB)/libssl.so ] ; then cd $(_3RDPLIB); ./config $(CONFIGURE_PARAMS); fi

build: config
	cd $(_3RDPLIB); make; 
	#cd $(_3RDPLIB); make test;

install: build
	if [ ! -f $(LIBDIR)/libssl.so ] ; then cd $(_3RDPLIB); make install; fi

clean:
	cd $(_3RDPLIB); make clean
