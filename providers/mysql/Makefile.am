bin_PROGRAMS = gda-mysql-srv

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	-I$(top_builddir)/lib \
	-I$(top_srcdir)/lib \
	$(MYSQL_CFLAGS)

gda_mysql_srv_SOURCES = \
	gda-mysql-provider.c \
	gda-mysql-provider.h \
	gda-mysql.h \
	main.c \
	utils.c
gda_mysql_srv_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(MYSQL_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_MySQL.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
