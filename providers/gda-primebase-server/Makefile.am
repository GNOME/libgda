bin_PROGRAMS = gda-primebase-srv
lib_LTLIBRARIES = libgda-primebase.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(PRIMEBASE_CFLAGS)

gda_primebase_srv_SOURCES = main-primebase.c
gda_primebase_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-primebase-server/libgda-primebase.la \
	$(PRIMEBASE_LIBS)

libgda_primebase_la_SOURCES = \
	gda-primebase-command.c \
	gda-primebase-connection.c \
	gda-primebase-recordset.c \
	gda-primebase.h

EXTRA_DIST = gda-primebase.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-primebase.oafinfo
