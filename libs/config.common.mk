# ******** GLOBAL DEFINITIONS ******** #

# Get **THIS** Makefile path:
# As make reads various makefiles, including any obtained from the MAKEFILES 
# variable, the command line, the default files, or from include directives, 
# their names will be automatically appended to the MAKEFILE_LIST variable. 
# They are added right before make begins to parse them. This means that if 
# the first thing a makefile does is examine the last word in this variable, 
# it will be the name of the current makefile. Once the current makefile has 
# used include, however, the last word will be the just-included makefile.
CONFIG_LINUX_MK_PATH_=$(shell pwd)/$(dir $(lastword $(MAKEFILE_LIST)))
GETABSPATH_SH= $(CONFIG_LINUX_MK_PATH_)/getabspath.sh
CONFIG_LINUX_MK_PATH= $(shell $(GETABSPATH_SH) $(CONFIG_LINUX_MK_PATH_))
LBITS := $(shell getconf LONG_BIT)

# ******** GLOBAL CONFIGURATIONS ******** #
CFLAGS+=-Wall -O3
ifeq ($(LBITS),64)
   # 64 bit stuff here
   CFLAGS+=-fPIC -fpic -D_LARGE_FILE_API -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
else
   # 32 bit stuff here
endif

#SHELL 
SHELL:=/bin/bash

