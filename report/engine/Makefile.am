bin_PROGRAMS = gda-report-engine

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	-I$(top_builddir)/report/common \
	$(GDA_CLIENT_CFLAGS) \
	-DGDA_LOCALEDIR=\""$(datadir)/locale\""

gda_report_engine_SOURCES = \
	gda-report-engine.c \
	gda-report-engine.h \
	main.c

gda_report_engine_LDADD = \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/report/common/libgdareport-common.la \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-client/libgda-client-2.la

serverdir=$(libdir)/bonobo/servers
server_in_files = GNOME_Database_Report.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA)
