bin_PROGRAMS = gda-mysql-srv-2

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(MYSQL_CFLAGS)

gda_mysql_srv_2_SOURCES = \
	gda-mysql-provider.c \
	gda-mysql-provider.h \
	gda-mysql-recordset.c \
	gda-mysql-recordset.h \
	gda-mysql.h \
	main.c \
	utils.c
gda_mysql_srv_2_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(MYSQL_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_MySQL.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
