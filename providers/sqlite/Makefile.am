bin_PROGRAMS = gda-sqlite-srv-2

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(SQLITE_CFLAGS)

gda_sqlite_srv_2_SOURCES = \
	gda-sqlite-provider.c \
	gda-sqlite-provider.h \
	gda-sqlite-recordset.c \
	gda-sqlite-recordset.h \
	gda-sqlite.h \
	main.c

gda_sqlite_srv_2_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(SQLITE_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_SQLite.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
