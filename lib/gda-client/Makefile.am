lib_LTLIBRARIES = libgda-client.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I.

gda_client_headers = \
	gda-client.h

libgda_client_la_SOURCES = $(gda_client_headers)
libgda_client_la_LIBADD =

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_client_headers)
