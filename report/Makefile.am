SUBDIRS = idl common engine

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	$(GDA_CLIENT_CFLAGS)

lib_LTLIBRARIES = libgda-report.la

gda_report_headers = \
	gda-report-client.h \
	gda-report.h
	
libgda_report_la_SOURCES = \
	$(gda_report_headers) \
	gda-report-client.c

libgda_report_la_LIBADD = \
	$(GDA_CLIENT_LIBS)

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_report_headers)

DTD_FILES=gda-report.dtd
dtddir=$(datadir)/gda/dtd
dtd_DATA=$(DTD_FILES)

