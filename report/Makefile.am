SUBDIRS = idl common engine

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/report/common \
	-I$(top_srcdir)/report/common \
	$(LIBGDA_CFLAGS)

lib_LTLIBRARIES = libgda-report.la

gda_report_headers = \
	gda-report-client.h \
	gda-report.h

libgda_report_la_SOURCES = \
	$(gda_report_headers) \
	gda-report-client.c

libgda_report_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_report_la_LIBADD = \
	$(LIBGDA_LIBS)

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_report_headers)

DTD_FILES=gda-report.dtd
dtddir=$(datadir)/gda/dtd
dtd_DATA=$(DTD_FILES)

