lib_LTLIBRARIES = libgda-common.la

SUBDIRS = gda-xml-query

INCLUDES = \
	$(GDA_CLIENT_CFLAGS) \
	$(GDA_PROVIDER_CFLAGS) \
	-I. \
	-DDTDINSTALLDIR=\""$(datadir)/gda/dtd"\"

gda_common_headers = \
	gda-config.h \
	gda-corba.h \
	gda-log.h \
	gda-thread.h \
	gda-util.h \
	gda-xml-database.h \
	gda-xml-file.h \
	gda-xml-query.h \
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
	gda-xml-file.c \
	gda-xml-query.c
libgda_common_la_LIBADD=$(GDA_CLIENT_LIBS)

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_common_headers)

DTD_FILES = gda-xml-query.dtd
dtddir=$(datadir)/gda/dtd
dtd_DATA=$(DTD_FILES)

EXTRA_DIST = $(DTD_FILES)
