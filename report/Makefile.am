SUBDIRS = idl libgda-report

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_builddir)/report \
	-I$(top_srcdir)/report \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-report-engine

gda_report_engine_SOURCES = \
	gda-report-engine.c \
	gda-report-engine.h \
	main.c

gda_report_engine_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(top_builddir)/report/libgda-report/libgda-report-2.la

DTD_FILES=gda-report.dtd
dtddir=$(datadir)/libgda/dtd
dtd_DATA=$(DTD_FILES)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Report.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)

