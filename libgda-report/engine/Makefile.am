noinst_LTLIBRARIES = libgda-report-engine-4.0.la

AM_CPPFLAGS = \
	-I$(top_builddir)/libgda-report \
	-I$(top_srcdir)/libgda-report \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	$(LIBGDA_CFLAGS)

gda_report_headers = \
	gda-report-engine.h

libgda_report_engine_4_0_la_SOURCES =	\
	$(gda_report_headers)	\
	gda-report-engine.c	

libgda_report_engine_4_0_la_LIBADD = $(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-4.0.la 

gdareportincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-report
gdareportinclude_HEADERS=$(gda_report_headers)
