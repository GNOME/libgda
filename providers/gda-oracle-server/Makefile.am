bin_PROGRAMS = gda-oracle-srv
noinst_LIBRARIES = libgda-oracle.a

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
	$(top_builddir)/providers/gda-oracle-server/libgda-oracle.a \
	$(ORACLE_LIBS)

libgda_oracle_a_SOURCES = \
	gda-oracle-command.c \
	gda-oracle-connection.c \
	gda-oracle-recordset.c \
	gda-oracle.h

oafdir=$(DESTDIR)$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_Oracle.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA) gda-oracle.spec.in
