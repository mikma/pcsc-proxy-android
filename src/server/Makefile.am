INCLUDES = -I../common $(pcsc_includes)

sbin_PROGRAMS=pcsc-proxy
pcsc_proxy_SOURCES=server.c
pcsc_proxy_LDADD=$(pcsc_libraries) $(pcsc_lib) $(LIBGNUTLS_LIBS) ../common/libcommon.la


