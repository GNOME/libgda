bin_PROGRAMS = gda-odbc-srv
noinst_LIBRARIES = libgda-odbc.a

INCLUDES = \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_srcdir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_srcdir)/lib/gda-server \
	-I$(top_builddir)/lib/gda-server \
	$(ODBC_CFLAGS)

gda_odbc_srv_SOURCES = main-odbc.c
gda_odbc_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-odbc-server/libgda-odbc.a \
	$(ODBC_LIBS)

libgda_odbc_a_SOURCES = \
	gda-odbc-command.c \
	gda-odbc-connection.c \
	gda-odbc-recordset.c \
	gda-odbc.h

oafdir=$(datadir)/oaf

oaf_in_files = GNOME_GDA_Provider_ODBC.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA) gda-odbc.spec.in
