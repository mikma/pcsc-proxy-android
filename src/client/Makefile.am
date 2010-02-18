INCLUDES = -I../common @pcsc_includes@

AM_CFLAGS=$(visibility_cflags) $(PTHREAD_CFLAGS)


lib_LTLIBRARIES=libpcsclite.la

noinst_HEADERS=client.h

libpcsclite_la_LDFLAGS=-no-undefined \
  -version-info 1:0:0

libpcsclite_la_SOURCES=client.c
libpcsclite_la_LIBADD=../common/libcommon.la $(PTHREAD_LIBS) $(LIBGNUTLS_LIBS)

noinst_PROGRAMS=pcsctest

pcsctest_SOPURCES=pcsctest.c
pcsctest_LDADD=libpcsclite.la

