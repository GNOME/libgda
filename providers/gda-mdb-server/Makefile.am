bin_PROGRAMS = gda-mdb-srv
noinst_LIBRARIES = libgda-mdb.a

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
	$(top_builddir)/providers/gda-mdb-server/libgda-mdb.a \
	$(MDB_LIBS)

libgda_mdb_a_SOURCES = \
	gda-mdb-command.c \
	gda-mdb-connection.c \
	gda-mdb-recordset.c \
	gda-mdb.h

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_MDB.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
