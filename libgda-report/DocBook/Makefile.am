noinst_LTLIBRARIES = libgda-report-docbook-6.0.la

AM_CPPFLAGS = \
	-I$(top_builddir)/libgda-report \
	-I$(top_srcdir)/libgda-report \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS)

gda_report_headers = \
	gda-report-docbook-document.h

libgda_report_docbook_6_0_la_SOURCES =	\
	$(gda_report_headers)	\
	gda-report-docbook-document.c	

libgda_report_docbook_6_0_la_LIBADD = $(COREDEPS_LIBS) \
	$(top_builddir)/libgda/libgda-6.0.la

gdareportincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-report
gdareportinclude_HEADERS=$(gda_report_headers)
