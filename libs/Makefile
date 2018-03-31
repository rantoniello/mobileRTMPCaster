#SHELL 
SHELL:=/bin/bash

# Get platform host
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	#ARCHS=arm x86 x86_device mips
	ARCHS=x86
	ARCHS_SUFFIX=_android
endif
ifeq ($(UNAME_S),Darwin)
	ARCHS=x86_64 armv7 arm64 i686
	ARCHS_SUFFIX=_darwin
endif

3RDPLIBS=3rdplibs

.PHONY : $(3RDPLIBS)

all: $(3RDPLIBS)

$(3RDPLIBS):
	@echo "#######################################################\n"
	@echo "########Configure and build 3rd party libraries########\n"
	@echo "#######################################################\n" 
	$(MAKE) 3rdplibs_libs
	if [[ $(UNAME_S) == Darwin ]]; then \
		rm -rf _install_dir_universal_darwin; \
		mkdir -p _install_dir_universal_darwin; \
		mkdir -p _install_dir_universal_darwin/lib; \
		mkdir -p _install_dir_universal_darwin/include; \
		cp -a _install_dir_armv7_darwin/include/* _install_dir_universal_darwin/include/; \
		cd _install_dir_armv7_darwin/lib; \
		for file in *.a; do \
			CREATE_UNIVERSAL_LIB=$$(for ARCH in $(ARCHS); do echo " -arch $${ARCH} _install_dir_$${ARCH}_darwin/lib/$$file"; done); \
			cd ../..; \
			echo Universal $$file being created…; \
			echo xcrun -sdk iphoneos lipo -output _install_dir_universal_darwin/lib/$$file -create $$CREATE_UNIVERSAL_LIB; \
			xcrun -sdk iphoneos lipo -output _install_dir_universal_darwin/lib/$$file -create $$CREATE_UNIVERSAL_LIB; \
			echo Universal $$file created.; cd -; \
		done; \
	fi

# NOTE: ‘$(ARCHS:.txt=.mytxt)’ replaces the extensions ‘.txt’ in $(ARCHS) by the new extension ‘.mytxt’
3rdplibs_libs: $(ARCHS:=__)

# NOTE: 
# ’%__’ below is used to write target with wildcards (e.g. ‘%__’ matches all targets that have that substring in ‘$(ARCHS:=__)’)
# ‘$*’ takes exactly the value of the substring matched by ‘%’ in the correspondent target itself. 

%__: %__/ffmpeg %__/nginx
	#@echo - $?, $@, $+, $<, $* # Debugging purposes
	#@echo

%__/ffmpeg: %__/x264 %__/vo-aacenc %__/lame
	$(MAKE) -C arch_$*$(ARCHS_SUFFIX)/ffmpeg all CROSS_ARCH=$*

%__/x264: %__/yasm
	$(MAKE) -C arch_$*$(ARCHS_SUFFIX)/x264 all CROSS_ARCH=$*

%__/yasm:
	@if [[ $* == *86* ]]; then \
		$(MAKE) -C arch_$*$(ARCHS_SUFFIX)/yasm all CROSS_ARCH=$*; \
	fi
%__/vo-aacenc:
	$(MAKE) -C arch_$*$(ARCHS_SUFFIX)/vo-aacenc all CROSS_ARCH=$*

%__/lame:
	$(MAKE) -C arch_$*$(ARCHS_SUFFIX)/lame all CROSS_ARCH=$*

%__/nginx: %__/openssl
	@if [[ $* == x86 ]]; then \
		$(MAKE) -C arch_$*$(ARCHS_SUFFIX)/nginx all CROSS_ARCH=$*; \
	fi
%__/openssl:
	@if [[ $* == x86 ]]; then \
		$(MAKE) -C arch_$*$(ARCHS_SUFFIX)/openssl all CROSS_ARCH=$*; \
	fi

clean:
	# 3rd party libraries
	@for ARCH in $(ARCHS); do \
		if ! [ -z arch_$${ARCH}$${ARCHS_SUFFIX} ]; then \
			$(MAKE) -C arch_$${ARCH}${ARCHS_SUFFIX}/ffmpeg clean; \
			$(MAKE) -C arch_$${ARCH}${ARCHS_SUFFIX}/x264 clean; \
			$(MAKE) -C arch_$${ARCH}${ARCHS_SUFFIX}/vo-aacenc clean; \
			$(MAKE) -C arch_$${ARCH}${ARCHS_SUFFIX}/lame clean; \
			if [[ $$ARCH == *86* ]]; then \
				$(MAKE) -C arch_$${ARCH}${ARCHS_SUFFIX}/yasm clean; \
			fi; \
			if [[ $$ARCH == x86 ]]; then \
				$(MAKE) -C arch_$${ARCH}${ARCHS_SUFFIX}/nginx clean; \
			fi; \
			if [[ $$ARCH == x86 ]]; then \
				$(MAKE) -C arch_$${ARCH}${ARCHS_SUFFIX}/openssl clean; \
			fi; \
		fi; \
	done
	rm -rf *~ *.log *.log*
	rm -rf _install_dir_*
