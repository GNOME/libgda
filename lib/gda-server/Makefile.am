lib_LTLIBRARIES = libgda-server.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I.

gda_server_headers = \
	gda-server.h

libgda_server_la_SOURCES = $(gda_server_headers)
libgda_server_la_LIBADD =

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_server_headers)
