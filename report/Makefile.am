SUBDIRS = idl server

INCLUDES = \
	$(GDA_CLIENT_CFLAGS)

lib_LTLIBRARIES = libgda-report.la

GDA_Report.h GDA_Report-common.c GDA_Report-stubs.c: $(top_builddir)/report/idl/GDA_Report.idl
	orbit-idl $(top_builddir)/report/idl/GDA_Report.idl

gda_report_headers = \
	gda-report-defs.h \
	gda-report-engine.h \
	gda-report.h

libgda_report_la_SOURCES = \
	GDA_Report.h \
	GDA_Report-common.c \
	GDA_Report-stubs.c \
	gda-report-engine.c \
	$(gda_report_headers)
libgda_report_la_LIBADD = \
	$(GDA_CLIENT_LIBS)

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_report_headers)

DTD_FILES=gda-report.dtd
dtddir=$(datadir)/gda/dtd
dtd_DATA=$(DTD_FILES)