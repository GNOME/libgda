bin_PROGRAMS = gda-report

INCLUDES = \
	-I$(top_builddir)/lib/gda-common \
	$(GDA_CLIENT_CFLAGS)

GDA_Report.h GDA_Report-common.c GDA_Report-skels.c: $(top_builddir)/report/idl/GDA_Report.idl
	$(ORBIT_IDL) $(top_builddir)/report/idl/GDA_Report.idl

gda_report_SOURCES = \
	GDA_Report.h \
	GDA_Report-common.c \
	GDA_Report-skels.c \
	gda-report-server-engine.c \
	gda-report-server-main.c \
	gda-report-server.h
gda_report_LDADD = \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-client/libgda-client.la
