bin_PROGRAMS = gda-postgres-srv
noinst_LIBRARIES = libgda-postgres.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(POSTGRES_CFLAGS)

gda_postgres_srv_SOURCES = main-postgres.c
gda_postgres_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-postgres-server/libgda-postgres.a \
	$(POSTGRES_LIBS)

libgda_postgres_a_SOURCES = \
	gda-postgres-command.c \
	gda-postgres-connection.c \
	gda-postgres-recordset.c \
	gda-postgres.h

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_Postgres.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA) gda-postgres.spec.in
