# Wrapper makefile for the third party package: libev

LIB_DEPENDENCIES = sxe-log
LIBRARIES        = ev
THIRD_PARTY.dir  = libev

include ../dependencies.mak

ifeq ($(OS),Windows_NT)
CFLAGS  = -c -DWINDOWS_NT=1 -DWIN32 -DFD_SETSIZE=2048 -Dinline=__inline $(IFLAGS) $(CC_INC)$(DST.dir)

# Compilation flags which sound good but are not found in the activestate build log for EV
#CFLAGS += -DDEV_SELECT_IS_WINSOCKET=1

# Compilation flags not found in the activestate build log for EV
CFLAGS += -DEV_STANDALONE=1
CFLAGS += -D_USE_32BIT_TIME_T
CFLAGS += -D_CRT_NONSTDC_NO_DEPRECATE
CFLAGS += -D_CRT_SECURE_NO_DEPRECATE
CFLAGS += -D_CRT_SECURE_NO_WARNINGS

# Compilation flags inspired by:
# http://ppm4.activestate.com/MSWin32-x86/5.10/1000/M/ML/MLEHMANN/EV-3.8.d/log-20090811T021343.txt
# and
# http://ppm4.activestate.com/MSWin32-x64/5.10/1000/M/ML/MLEHMANN/EV-3.8.d/log-20090810T125825.txt
ifdef MAKE_MINGW
CFLAGS += -DMAKE_MINGW
else
CFLAGS += -GF
CFLAGS += -W3
#Note: using /MT or /MTd instead: CFLAGS += -MD
CFLAGS += -Z7
endif
CFLAGS += -DDEBUG
#CFLAGS += -DNDEBUG
#defined globally now as O2 CFLAGS += -O1
CFLAGS += -DWIN32
CFLAGS += -D_CONSOLE
CFLAGS += -DHAVE_DES_FCRYPT
CFLAGS += -DVERSION=\"3.8\"
CFLAGS += -DEV_USE_MONOTONIC=1
CFLAGS += -DEV_USE_REALTIME=0
CFLAGS += -DEV_USE_SELECT=1
CFLAGS += -DEV_USE_POLL=0
CFLAGS += -DEV_USE_EPOLL=0
CFLAGS += -DEV_USE_KQUEUE=0
CFLAGS += -DEV_USE_PORT=0
CFLAGS += -DEV_USE_INOTIFY=0
CFLAGS += -DEV_USE_EVENTFD=0
CFLAGS += -DEV_USE_SIGNALFD=0
endif

release debug coverage:		$(DST.dir)/ev.h

$(info make[$(MAKELEVEL)]: todo:     use Perl EV module test suite to test our freshly built libev)

ifeq ($(OS),Windows_NT)

# On Windows, we make it ourselves

$(DST.dir)/ev$(EXT.lib):	$(DST.dir)/ev$(EXT.obj)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(LIB_CMD) $(LIB_FLAGS) $(LIB_OUT)$@ $^

$(DST.dir)/ev$(EXT.obj):	$(DST.dir)/$(THIRD_PARTY.dir)/ev.c $(DST.dir)/ev.h
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	@echo       $(CC) $(CFLAGS) $< $(CC_OUT)$@ >  $(call OSPATH,$@.out) 2>&1
	$(MAKE_RUN) $(CC) $(CFLAGS) $< $(CC_OUT)$@ >> $(call OSPATH,$@.out) 2>&1 $(CC_OUT_ON_ERROR)

else

# On UNIX like OSs, use autoconf

$(DST.dir)/ev$(EXT.lib):	$(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/.libs/libev$(EXT.lib)
ifdef MAKE_CACHE_THIRD_PARTY
	$(MAKE_RUN) $(MKDIR) $(DST.dir)/$(THIRD_PARTY.dir)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: copying:  $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/ev$(EXT.obj) $(DST.dir)/$(THIRD_PARTY.dir)/ev$(EXT.obj)"
	$(MAKE_RUN) $(COPY) $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/ev$(EXT.obj) $(DST.dir)/$(THIRD_PARTY.dir)/ev$(EXT.obj)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: copying:  $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/event$(EXT.obj) $(DST.dir)/$(THIRD_PARTY.dir)/event$(EXT.obj)"
	$(MAKE_RUN) $(COPY) $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/event$(EXT.obj) $(DST.dir)/$(THIRD_PARTY.dir)/event$(EXT.obj)
endif
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: copying:  $@"
	$(MAKE_RUN) $(COPY) $^ $@

$(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/.libs/libev$(EXT.lib):   $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/Makefile
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: copying:  misc sxe header files into third party build folder"
	$(MAKE_RUN) $(COPY) ../lib-sxe-log/sxe-log.h                  $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)
	$(MAKE_RUN) $(COPY) ../lib-sxe-log/sxe-log-legacy.h           $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)
	$(MAKE_RUN) $(COPY) ../lib-sxe-log/$(DST.dir)/sxe-*-proto.h   $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) $(MAKE) -C $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)

ifneq ($(filter debug,$(MAKECMDGOALS)),)
SUBMAKE_CFLAGS = ${CFLAGS_OPT_OFF}
else
SUBMAKE_CFLAGS = ${CFLAGS_OPT_ON}
endif

ifeq ($(OS_bits),32)
LIBEV_CONFIGURE_PREFIX  = env CFLAGS="-m32 ${SUBMAKE_CFLAGS}" LDFLAGS=-m32
LIBEV_CONFIGURE_POSTFIX = --build=i686-unknown-linux-gnu --disable-ld64
else
LIBEV_CONFIGURE_PREFIX  = env CFLAGS="${SUBMAKE_CFLAGS}"
LIBEV_CONFIGURE_POSTFIX =
endif

$(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/Makefile:			$(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/configure
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: building: $@"
	$(MAKE_RUN) cd $(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir) && $(LIBEV_CONFIGURE_PREFIX) ./configure $(LIBEV_CONFIGURE_POSTFIX)

endif

$(DST.dir)/ev.h:			$(THIRD_PARTY_DST.dir)/$(THIRD_PARTY.dir)/ev.h
	@$(MAKE_PERL_ECHO) "make[$(MAKELEVEL)]: copying:  $@"
	$(MAKE_RUN) $(COPY) $(call OSPATH,$^) $(call OSPATH,$@)

