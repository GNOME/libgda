bin_PROGRAMS = gda-mdb-srv
lib_LTLIBRARIES = libgda-mdb.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(MDB_CFLAGS)

gda_mdb_srv_SOURCES = main-mdb.c
gda_mdb_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-mdb-server/libgda-mdb.la \
	$(MDB_LIBS)

libgda_mdb_la_SOURCES = \
	gda-mdb-command.c \
	gda-mdb-connection.c \
	gda-mdb-recordset.c \
	gda-mdb.h

EXTRA_DIST = GNOME_GDA_Provider_MDB.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=GNOME_GDA_Provider_MDB.oafinfo
