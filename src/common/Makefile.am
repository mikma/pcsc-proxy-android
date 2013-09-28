INCLUDES = $(pcsc_includes)

AM_CFLAGS=$(visibility_cflags)

noinst_LTLIBRARIES=libcommon.la

libcommon_la_SOURCES=\
  netopts.c \
  network.c \
  message.c \
  unix.c

libcommon_la_LIBADD=

if LIBGNUTLS
INCLUDES += $(LIBGNUTLS_CFLAGS)
libcommon_la_SOURCES += tls.c
libcommon_la_LIBADD += $(LIBGNUTLS_LIBS)
endif

if OPENSSL
INCLUDES += $(OPENSSL_CFLAGS)
libcommon_la_SOURCES += openssl.c
libcommon_la_LIBADD += $(OPENSSL_LIBS)
endif

if BLUETOOTH
INCLUDES += $(BLUETOOTH_CFLAGS)
libcommon_la_SOURCES += bluetooth.c
libcommon_la_LIBADD += $(BLUETOOTH_LIBS)
endif

noinst_HEADERS=\
  message.h \
  network.h \
  tls.h \
  bluetooth.h \
  unix.h

