LIBRARIES        = sxe-test

include ../dependencies.mak

# Special case: usually only ./test/*.c files include tap.h
ifdef SXE_EMBEDDED_TAP
    IFLAGS += $(CC_INC)$(COM.dir)/../libsxe/lib-tap/$(DST.dir)
endif
