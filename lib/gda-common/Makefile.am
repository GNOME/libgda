lib_LTLIBRARIES = libgda-common.la

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I.

gda_common_headers = \
	gda-common.h

libgda_common_la_SOURCES = \
	$(gda_common_headers)

libgda_common_la_LIBADD =

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_common_headers)
