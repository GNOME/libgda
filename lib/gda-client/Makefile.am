lib_LTLIBRARIES = libgda-client-2.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	-I. \
	-I$(top_builddir)/lib/gda-common \
	-DDTDINSTALLDIR=\""$(datadir)/gda/dtd"\"

gda_client_headers = \
	gda-client.h \
	gda-connection.h

libgda_client_2_la_SOURCES = \
	$(gda_client_headers) \
	gda-client.c \
	gda-connection.c
libgda_client_2_la_LIBADD =

gdaincludedir=$(includedir)/libgda-$(VERSION)/gda
gdainclude_HEADERS=$(gda_client_headers)

