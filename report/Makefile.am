SUBDIRS = idl server

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	$(GDA_CLIENT_CFLAGS)

lib_LTLIBRARIES = libgda-report.la

BUILT_SOURCES = GDA_Report.h GDA_Report-common.c GDA_Report-stubs.c GDA_Report-skels.c
CLEANFILES += $(BUILT_SOURCES) gda-report.dtd

$(BUILT_SOURCES): $(top_builddir)/report/idl/GDA_Report.idl
	orbit-idl $(top_builddir)/report/idl/GDA_Report.idl

gda_report_headers = \
	gda-report-defs.h \
	gda-report-engine.h \
	gda-report-stream.h \
	gda-report-client.h

libgda_report_la_SOURCES = \
	GDA_Report.h \
	GDA_Report-common.c \
	GDA_Report-stubs.c \
	gda-report-engine.c \
	gda-report-stream.c \
	$(gda_report_headers)
libgda_report_la_LIBADD = \
	$(GDA_CLIENT_LIBS)

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_report_headers)

DTD_FILES=gda-report.dtd
dtddir=$(datadir)/gda/dtd
dtd_DATA=$(DTD_FILES)
