bin_PROGRAMS = gda-odbc-srv-2

INCLUDES = \
	$(LIBGDA_CFLAGS) \
	$(ODBC_CFLAGS)

gda_odbc_srv_2_SOURCES = \
	gda-odbc-provider.c \
	gda-odbc-provider.h \
	gda-odbc.h \
	main.c
gda_odbc_srv_2_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(ODBC_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_ODBC.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)