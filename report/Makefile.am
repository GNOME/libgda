SUBDIRS = idl engine

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	$(GDA_CLIENT_CFLAGS)

lib_LTLIBRARIES = libgda-report.la

BUILT_SOURCES = \
	GNOME_Database_Report.h \
	GNOME_Database_Report-common.c \
	GNOME_Database_Report-stubs.c \
	GNOME_Database_Report-skels.c

CLEANFILES += $(BUILT_SOURCES) gda-report.dtd

$(BUILT_SOURCES): $(top_builddir)/report/idl/GNOME_Database_Report.idl
	orbit-idl $(top_builddir)/report/idl/GNOME_Database_Report.idl

gda_report_headers = \
	gda-report-defs.h \
	gda-report-format.h \
	gda-report-stream.h \
	gda-report-output.h \
	gda-report-engine.h \
	gda-report.h \
	gda-report-section.h \
	gda-report-header.h \
	gda-report-page.h \
	gda-report-page-header.h \
	gda-report-data-header.h \
	gda-report-group-header.h \
	gda-report-detail.h \
	gda-report-data.h \
	gda-report-element.h \
	gda-report-picture.h \
	gda-report-line.h \
	gda-report-object.h \
	gda-report-text-object.h \
	gda-report-label.h \
	gda-report-special.h \
	gda-report-repfield.h \
	gda-report-group-footer.h \
	gda-report-data-footer.h \
	gda-report-page-footer.h \
	gda-report-footer.h
	
libgda_report_la_SOURCES = \
	$(BUILT_SOURCES) \
	gda-report-engine.c \
	gda-report-stream.c \
	gda-report.c \
	$(gda_report_headers)

libgda_report_la_LIBADD = \
	$(GDA_CLIENT_LIBS)

gdaincludedir=$(includedir)/gda
gdainclude_HEADERS=$(gda_report_headers)

DTD_FILES=gda-report.dtd
dtddir=$(datadir)/gda/dtd
dtd_DATA=$(DTD_FILES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)
