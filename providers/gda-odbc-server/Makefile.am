bin_PROGRAMS = gda-odbc-srv
lib_LTLIBRARIES = libgda-odbc.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(ODBC_CFLAGS)

gda_odbc_srv_SOURCES = main-odbc.c
gda_odbc_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-odbc-server/libgda-odbc.la \
	$(ODBC_LIBS)

libgda_odbc_la_SOURCES = \
	gda-odbc-command.c \
	gda-odbc-connection.c \
	gda-odbc-recordset.c \
	gda-odbc.h

EXTRA_DIST = gda-odbc.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-odbc.oafinfo
