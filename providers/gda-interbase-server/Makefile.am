bin_PROGRAMS = gda-interbase-srv
lib_LTLIBRARIES = libgda-interbase.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server

gda_interbase_srv_SOURCES = main-interbase.c
gda_interbase_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-interbase/libgda-interbase.la

libgda_interbase_SOURCES = \
	gda-interbase-command.c \
	gda-interbase-connection.c \
	gda-interbase-recordset.c \
	gda-interbase.h

EXTRA_DIST = gda-interbase.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-interbase.oafinfo
