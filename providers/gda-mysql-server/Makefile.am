bin_PROGRAMS = gda-mysql-srv
lib_LTLIBRARIES = libgda-mysql.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server

gda_mysql_SOURCES = main-mysql.c
gda_mysql_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-mysql-server/libgda-mysql.la

libgda_mysql_SOURCES = \
	gda-mysql-command.c \
	gda-mysql-connection.c \
	gda-mysql-recordset.c \
	gda-mysql.h

EXTRA_DIST = gda-mysql.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-mysql.oafinfo
