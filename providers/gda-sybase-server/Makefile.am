bin_PROGRAMS = gda-sybase-srv
lib_LTLIBRARIES = libgda-sybase.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server

gda_sybase_srv_SOURCES = main-sybase.c
gda_sybase_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-sybase-server/libgda-sybase.la

libgda_sybase_SOURCES = \
	gda-sybase-command.c \
	gda-sybase-command.h \
	gda-sybase-connection.c \
	gda-sybase-connection.h \
	gda-sybase-error.c \
	gda-sybase-error.h \
	gda-sybase-recordset.c \
	gda-sybase-recordset.h \
	gda-sybase-types.c \
	gda-sybase-types.h \
	gda-sybase.h

EXTRA_DIST = gda-sybase.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-sybase.oafinfo
