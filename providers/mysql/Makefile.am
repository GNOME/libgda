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

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_MySQL.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA) gda-mysql.spec.in
