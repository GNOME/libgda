bin_PROGRAMS = gda-tds-srv
noinst_LIBRARIES = libgda-tds.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(TDS_CFLAGS)

gda_tds_srv_SOURCES = main-tds.c
gda_tds_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-tds-server/libgda-tds.a \
	$(TDS_LIBS)

libgda_tds_a_SOURCES = \
	gda-tds-command.c \
	gda-tds-command.h \
	gda-tds-connection.c \
	gda-tds-connection.h \
	gda-tds-error.c \
	gda-tds-error.h \
	gda-tds-recordset.c \
	gda-tds-recordset.h \
	gda-tds-types.c \
	gda-tds-types.h \
	gda-tds.h

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_TDS.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
