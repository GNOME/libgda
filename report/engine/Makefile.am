bin_PROGRAMS = gda-report-engine

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
	$(ORBIT_IDL) $(IDL_FLAGS) $(top_builddir)/report/idl/GDA_Report.idl

gda_report_engine_SOURCES = \
	$(BUILT_SOURCES) \
	gda-report-document-factory.h \
	gda-report-document-factory.c \
	main.c

gda_report_engine_LDADD = \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-client/libgda-client.la

serverdir=$(GDA_serverdir)
server_in_files = GNOME_Database_Report.server.in
server_DATA = $(server_in_files:.server.in=.server)
@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA)
