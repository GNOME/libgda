lib_LTLIBRARIES = libgda-report-2.la

INCLUDES = \
	-I$(top_builddir)/report \
	-I$(top_srcdir)/report \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	$(LIBGDA_CFLAGS) \
	-DLIBGDA_REPORT_DTDDIR=\""$(datadir)/libgda/xml"\"


gda_report_headers = \
	gda-report-document.h \
	gda-report-valid.h \
	gda-report-types.h \
	gda-report-item.h \
	gda-report-item-report.h \
	gda-report-item-sqlquery.h \
	gda-report-item-reportheader.h \
	gda-report-item-reportfooter.h \
	gda-report-item-pageheader.h \
	gda-report-item-pagefooter.h \
	gda-report-item-detail.h \
	gda-report-item-label.h \
	gda-report-item-repfield.h \
	gda-report-result.h \
	gda-report.h

libgda_report_2_la_SOURCES =	\
	$(gda_report_headers)	\
	gda-report-document.c	\
	gda-report-valid.c	\
	gda-report-types.c	\
	gda-report-item.c	\
	gda-report-item-report.c \
	gda-report-item-sqlquery.c \
	gda-report-item-reportheader.c \
	gda-report-item-reportfooter.c  \
	gda-report-item-pageheader.c \
	gda-report-item-pagefooter.c \
	gda-report-item-detail.c \
	gda-report-item-label.c \
	gda-report-item-repfield.c \
	gda-report-result.c
	

libgda_report_2_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_report_2_la_LIBADD = $(LIBGDA_LIBS) $(top_builddir)/libgda/libgda-2.la

gdareportincludedir=$(includedir)/libgda-report
gdareportinclude_HEADERS=$(gda_report_headers)

