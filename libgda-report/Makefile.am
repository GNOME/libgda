lib_LTLIBRARIES = libgda-report-4.0.la

SUBDIRS = engine DocBook RML

AM_CPPFLAGS = \
	-I$(top_builddir)/libgda-report \
	-I$(top_srcdir)/libgda-report \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS) 

report_headers = \
	gda-report-document.h

libgda_report_4_0_la_SOURCES = \
	$(report_headers) \
	gda-report-document-private.h \
	gda-report-document.c

libgda_report_4_0_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE) $(NO_UNDEFINED)
libgda_report_4_0_la_LIBADD = engine/libgda-report-engine-4.0.la \
	DocBook/libgda-report-docbook-4.0.la \
	RML/libgda-report-rml-4.0.la \
	$(LIBGDA_LIBS)

gdareportincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-report
gdareportinclude_HEADERS= libgda-report.h $(report_headers)
