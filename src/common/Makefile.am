INCLUDES = $(pcsc_includes)

AM_CFLAGS=$(visibility_cflags)

noinst_LTLIBRARIES=libcommon.la

libcommon_la_SOURCES=\
  network.c \
  message.c

if LIBGNUTLS
INCLUDES += $(LIBGNUTLS_CFLAGS)
libcommon_la_SOURCES += tls.c
libcommon_la_LIBADD = $(LIBGNUTLS_LIBS)
endif

if OPENSSL
INCLUDES += $(OPENSSL_LIBS)
libcommon_la_SOURCES += openssl.c
libcommon_la_LIBADD = $(OPENSSL_LIBS)
endif

noinst_HEADERS=\
  message.h \
  network.h \
  tls.h

