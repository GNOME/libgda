lib_LTLIBRARIES = libgda-common.la

SUBDIRS = gda-xml-query

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I.

gda_common_headers = \
	gda-config.h \
	gda-corba.h \
	gda-log.h \
	gda-thread.h \
	gda-util.h \
	gda-xml-database.h \
	gda-xml-file.h \
	gda-common.h

libgda_common_la_SOURCES = \
	$(gda_common_headers) \
	gda-config.c \
	gda-corba.c \
	gda-init.c \
	gda-log.c \
	gda-thread.c \
	gda-util.c \
	gda-xml-database.c \
	gda-xml-file.c

libgda_common_la_LIBADD=\
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/gda-xml-query/libxmlquery.la

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_common_headers)
