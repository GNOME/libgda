lib_LTLIBRARIES = libgda-report-2.la

INCLUDES = \
	-I$(top_builddir)/report \
	-I$(top_srcdir)/report \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBGDA_CFLAGS)

gda_report_headers = \
	gda-report-document.h \
	gda-report-types.h \
	gda-report.h

libgda_report_2_la_SOURCES = \
	$(gda_report_headers) \
	gda-report-document.c

libgda_report_2_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_report_2_la_LIBADD = $(LIBGDA_LIBS)

gdareportincludedir=$(includedir)/libgda-report
gdareportinclude_HEADERS=$(gda_report_headers)

