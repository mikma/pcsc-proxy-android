INCLUDES = -I../common $(pcsc_includes)

sbin_PROGRAMS=pcsc-proxy
pcsc_proxy_SOURCES=server.c
pcsc_proxy_LDADD=$(pcsc_libraries) $(pcsc_lib) ../common/libcommon.la

if LIBGNUTLS
pcsc_proxy_LDADD += $(LIBGNUTLS_LIBS)
endif

if OPENSSL
pcsc_proxy_LDADD += $(OPENSSL_LIBS)
endif
