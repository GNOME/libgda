lib_LTLIBRARIES = libgda-server-2.la

INCLUDES = \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common

gda_server_headers = \
	gda-server-connection.h \
	gda-server-provider.h \
	gda-server.h 

libgda_server_2_la_SOURCES = \
	$(gda_server_headers) \
	gda-server-connection.c \
	gda-server-provider.c \
	gda-server.c 
libgda_server_2_la_LIBADD = -lpopt

gdaincludedir=$(includedir)/libgda-$(VERSION)/gda
gdainclude_HEADERS=$(gda_server_headers)

