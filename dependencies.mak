# Copyright (c) 2010 Sophos Group.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

# Find the relative path to the component directory and to the top of the hierarchy

COM.dir := $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
TOP.dir = $(COM.dir)/..

# Defined if internal tap if to be used. Use lazy binding to allow SXE_EXTERNAL_TAP to be defined later
TAP     = $(if $(SXE_EXTERNAL_TAP),,tap)
LIB_TAP = $(if $(SXE_EXTERNAL_TAP),,lib-tap)

# List of the libraries in linker order.
# This is used by both the package GNUmakefiles and the top level GNUmakefile
# This can be overridden by parent GNUmakefiles if desired
#
remove_to = $(if $(filter $(1),$(2)),$(call remove_to,$(1),$(wordlist 2,$(words $(2)),$(2))),$(2))
ALL_LIBRARIES ?= sxe-dirwatch sxe-ring-buffer sxe-httpd sxe-http sxe-sync-ev sxe-pool-tcp sxe-jitson sxe-cdb sxe-hash lookup3 \
                 murmurhash3 sha1 sxe-spawn sxe sxe-pool sxe-thread sxe-mmap sxe-buffer sxe-list sxe-socket \
                 ev sxe-cstr sxe-util sxe-log sxe-test mock port $(TAP)
LIB_DEPENDENCIES = $(call remove_to,$(LIBRARIES),$(ALL_LIBRARIES))

# Convention opt-out list
CONVENTION_OPTOUT_LIST = lib-lookup3 lib-mock lib-port
MAKE_ALLOW_SPACE_AFTER_ASTERISK  = 1    # Much of lib-sxe puts all declarations on separate lines, so it doesn't cuddle asterisks
MAKE_ALLOW_LOWERCASE_HASH_DEFINE = 1    # Allow lower case defines for sxe-alloc.h wrappers

# Coverage opt-out list
COVERAGE_OPTOUT_LIST   = lib-lookup3 lib-murmurhash3 lib-mock lib-port lib-sha1 $(LIB_TAP)

include $(TOP.dir)/mak/mak-common.mak

ifneq ($(MAK_VERSION),1)    # Versions of mak > 1 use an external tap libary
    SXE_EXTERNAL_TAP = 1
endif

IFLAGS += $(if $(findstring port,$(LIB_DEPENDENCIES)),$(CC_INC)$(COM.dir)/lib-port/$(OS_class),)

ifeq ($(OS),Windows_NT)
ifdef MAKE_MINGW
    LINK_FLAGS += -lWinmm
else
    LINK_FLAGS += /DEFAULTLIB:Winmm.lib
endif
else
    LINK_FLAGS += -lrt    # Add -lcrypto if SXE_DISABLE_OPENSSL is not set
endif

# Add -DSXE_DISABLE_XXHASH=1 to remove dependency on xxhash
CFLAGS += -DSXE_DISABLE_OPENSSL=1
