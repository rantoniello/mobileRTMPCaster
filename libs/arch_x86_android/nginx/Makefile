include ../../config.common.mk
include ../../config.android-x86.mk

BUILD_DIR := $(shell pwd)/build_dir

NGINX_DIR := $(shell pwd)/../../3rdplibs/nginx/nginx-1.4.0
RTMPMOD_DIR := $(shell pwd)/../../3rdplibs/nginx/nginx-rtmp-module-1.1.3
PCRE_DIR := $(shell pwd)/../../3rdplibs/nginx/pcre-8.35
OPENSSL_DIR := $(shell pwd)/../openssl/openssl-1.0.1h
export LDFLAGS := -L$(INSTALL_DIR)/lib
export CFLAGS := -I$(INSTALL_DIR)/include

CONFIGURE_PARAMS= --prefix=${INSTALL_DIR} --builddir=${BUILD_DIR} \
			--with-openssl=${OPENSSL_DIR} --add-module=${RTMPMOD_DIR} --with-http_ssl_module \
			--with-pcre=${PCRE_DIR} --with-cc-opt=-I$(INSTALL_DIR)/include --with-ld-opt=-L$(INSTALL_DIR)/lib

all: install

config:
	mkdir -p $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/include
	mkdir -p $(BUILD_DIR)
#	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd $(BUILD_DIR); cp -a ../${NGINX_DIR}/auto . ; ../${NGINX_DIR}/configure $(CONFIGURE_PARAMS); fi
	if [ ! -f $(BUILD_DIR)/Makefile ] ; then cd ${NGINX_DIR}; ./configure $(CONFIGURE_PARAMS); fi

build: config
	cd ${NGINX_DIR}; make

install: build
	cd ${NGINX_DIR}; make install

clean:
	if [ -f $(BUILD_DIR)/Makefile ] ; then \
                cd ${NGINX_DIR}; make clean; \
        fi

