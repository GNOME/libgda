lib_LTLIBRARIES = libgda-report-2.la

INCLUDES = \
	-I$(top_builddir)/report \
	-I$(top_srcdir)/report \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS)

BUILT_SOURCES = \
	GNOME_Database_Report-skels.c \
	GNOME_Database_Report-stubs.c \
	GNOME_Database_Report-common.c \
	GNOME_Database_Report.h

CLEANFILES += $(BUILT_SOURCES)
IDL_FILES = \
	$(top_builddir)/report/idl/GNOME_Database_Report.idl

gda_report_corba_built_files = \
	GNOME_Database_Report-skels.c \
	GNOME_Database_Report-stubs.c \
	GNOME_Database_Report-common.c \
	GNOME_Database_Report.h

$(gda_report_corba_built_files): $(IDLFILES) $(ORBIT_IDL)
	$(ORBIT_IDL) -I$(top_builddir)/idl -I$(BONOBO_ACTIVATION_IDL_DIR) -I$(BONOBO_IDL_DIR) -I$(top_builddir)/idl $(top_builddir)/report/idl/GNOME_Database_Report.idl

gda_report_headers = \
	gda-report-client.h \
	gda-report.h

libgda_report_2_la_SOURCES = \
	$(gda_report_corba_built_files) \
	$(gda_report_headers) \
	gda-report-client.c

libgda_report_2_la_LDFLAGS = -version-info $(GDA_CURRENT):$(GDA_REVISION):$(GDA_AGE)
libgda_report_2_la_LIBADD = $(LIBGDA_LIBS)

gdareportincludedir=$(includedir)/libgda-report
gdareportinclude_HEADERS=$(gda_report_headers)

CLEANFILES = $(gda_report_corba_built_files)

dist-hook:
	cd $(distdir); rm -f $(gda_report_corba_built_files)
