bin_PROGRAMS = gda-oracle-srv
lib_LTLIBRARIES = libgda-oracle.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(ORACLE_CFLAGS)

gda_oracle_srv_SOURCES = main-oracle.c
gda_oracle_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-oracle-server/libgda-oracle.la \
	$(ORACLE_LIBS)

libgda_oracle_la_SOURCES = \
	gda-oracle-command.c \
	gda-oracle-connection.c \
	gda-oracle-recordset.c \
	gda-oracle.h

EXTRA_DIST = gda-oracle.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-oracle.oafinfo
