INCLUDES = @pcsc_includes@ @LIBGNUTLS_CFLAGS@

AM_CFLAGS=$(visibility_cflags)

noinst_LTLIBRARIES=libcommon.la

libcommon_la_LIBADD=@LIBGNUTLS_LIBS@

libcommon_la_SOURCES=\
  network.c \
  message.c

if FALSE
libcommon_la_SOURCES += openssl.c
else
libcommon_la_SOURCES += tls.c
endif

noinst_HEADERS=\
  message.h \
  network.h \
  tls.h

