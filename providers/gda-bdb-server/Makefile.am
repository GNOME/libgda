bin_PROGRAMS = gda-bdb-srv
noinst_LIBRARIES = libgda-bdb.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server

gda_bdb_srv_SOURCES = main-bdb.c
gda_bdb_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-postgres-server/libgda-bdb.a

libgda_bdb_a_SOURCES = \
	gda-bdb-command.c \
	gda-bdb-connection.c \
	gda-bdb-recordset.c \
	gda-bdb.h

oafdir=$(DESTDIR)$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_BDB.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
