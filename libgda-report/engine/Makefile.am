noinst_LTLIBRARIES = libgda-report-engine-4.0.la
noinst_PROGRAMS = test-rt-parser

AM_CPPFLAGS = \
	-I$(top_builddir)/libgda-report \
	-I$(top_srcdir)/libgda-report \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
	$(LIBGDA_CFLAGS) \
	$(GDKPIXBUF_CFLAGS) \
	$(LIBGDA_WFLAGS)

gda_report_headers = \
	gda-report-engine.h

libgda_report_engine_4_0_la_SOURCES =	\
	$(gda_report_headers) \
	rt-parser.h \
	rt-parser.c \
	gda-report-engine.c	

libgda_report_engine_4_0_la_LIBADD = $(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-4.0.la 

test_rt_parser_SOURCES = \
	test-rt-parser.c

test_rt_parser_LDADD = libgda-report-engine-4.0.la $(GDKPIXBUF_LIBS)

gdareportincludedir=$(includedir)/libgda-$(GDA_ABI_MAJOR_VERSION).$(GDA_ABI_MINOR_VERSION)/libgda-report
gdareportinclude_HEADERS=$(gda_report_headers)
