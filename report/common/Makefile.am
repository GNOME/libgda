noinst_LTLIBRARIES = libgdareport-common.la

BUILT_SOURCES = \
	GNOME_Database_Report.h \
	GNOME_Database_Report-common.c \
	GNOME_Database_Report-stubs.c \
	GNOME_Database_Report-skels.c

CLEANFILES += $(BUILT_SOURCES) gda-report.dtd

$(BUILT_SOURCES): $(top_builddir)/report/idl/GNOME_Database_Report.idl
	orbit-idl -I$(BONOBO_ACTIVATION_IDL_DIR) $(top_builddir)/report/idl/GNOME_Database_Report.idl

gda_report_headers = \
	GNOME_Database_Report.h

libgdareport_common_la_SOURCES = \
	$(gda_report_headers) \
	gda-report-common.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)