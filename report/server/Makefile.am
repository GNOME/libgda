bin_PROGRAMS = gda-report-server

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	-I$(top_builddir)/report \
	$(GDA_CLIENT_CFLAGS)

GDA_Report.h GDA_Report-common.c GDA_Report-skels.c: $(top_builddir)/report/idl/GDA_Report.idl
	$(ORBIT_IDL) $(top_builddir)/report/idl/GDA_Report.idl

gda_report_server_SOURCES = \
	GDA_Report.h \
	GDA_Report-common.c \
	GDA_Report-skels.c \
	gda-report-server-cache.c \
	gda-report-server-cache.h \
	gda-report-server-db.c \
	gda-report-server-db.h \
	gda-report-server-element.c \
	gda-report-server-engine.c \
	gda-report-server-format.c \
	gda-report-server-main.c \
	gda-report-server-stream.c \
	gda-report-server.h
gda_report_server_LDADD = \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-client/libgda-client.la

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-report.oafinfo

EXTRA_DIST = gda-report.oafinfo
