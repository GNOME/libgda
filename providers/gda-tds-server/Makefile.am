bin_PROGRAMS = gda-tds-srv
lib_LTLIBRARIES = libgda-tds.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(TDS_CFLAGS)

gda_tds_srv_SOURCES = main-tds.c
gda_tds_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-tds-server/libgda-tds.la \
	$(TDS_LIBS)

libgda_tds_la_SOURCES = \
	gda-tds-command.c \
	gda-tds-command.h \
	gda-tds-connection.c \
	gda-tds-connection.h \
	gda-tds-error.c \
	gda-tds-error.h \
	gda-tds-recordset.c \
	gda-tds-recordset.h \
	gda-tds-types.c \
	gda-tds-types.h \
	gda-tds.h

EXTRA_DIST = gda-tds.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-tds.oafinfo
