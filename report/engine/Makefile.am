bin_PROGRAMS = gda-report-server

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	-I$(top_builddir)/report \
	$(GDA_CLIENT_CFLAGS)

BUILT_SOURCES = \
	GDA_Report.h \
	GDA_Report-common.c \
	GDA_Report-stubs.c \
	GDA_Report-skels.c

CLEANFILES += $(BUILT_SOURCES)

$(BUILT_SOURCES): $(top_builddir)/report/idl/GDA_Report.idl
	$(ORBIT_IDL) $(top_builddir)/report/idl/GDA_Report.idl

gda_report_server_SOURCES = \
	$(BUILT_SOURCES) \
	gda-report-document-factory.h \
	main.c

gda_report_server_LDADD = \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-client/libgda-client.la

oafdir=$(GDA_oafdir)
oaf_in_files = GNOME_GDA_Report.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)
@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST = $(oaf_in_files) $(oaf_DATA)
