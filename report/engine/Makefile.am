bin_PROGRAMS = gda-report-engine

INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
	-I$(top_builddir)/report/common \
	-I$(top_srcdir)/report/common \
	$(LIBGDA_CFLAGS) \
	-DGDA_LOCALEDIR=\""$(datadir)/locale\""

gda_report_engine_SOURCES = \
	gda-report-engine.c \
	gda-report-engine.h \
	main.c

gda_report_engine_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/report/common/libgdareport-common.la \
	$(top_builddir)/libgda/libgda2.la

serverdir=$(libdir)/bonobo/servers
server_in_files = GNOME_Database_Report.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA)
