bin_PROGRAMS = gda-mysql-srv
noinst_LIBRARIES = libgda-mysql.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(MYSQL_CFLAGS)

gda_mysql_srv_SOURCES = main-mysql.c
gda_mysql_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-mysql-server/libgda-mysql.a \
	$(MYSQL_LIBS)

libgda_mysql_a_SOURCES = \
	gda-mysql-command.c \
	gda-mysql-connection.c \
	gda-mysql-recordset.c \
	gda-mysql.h 

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_MySQL.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA) gda-mysql.spec.in
