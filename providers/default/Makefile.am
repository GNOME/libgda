bin_PROGRAMS = gda-default-srv-2

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(SQLITE_CFLAGS)

gda_default_srv_2_SOURCES = \
	gda-default-provider.c \
	gda-default-provider.h \
	gda-default-recordset.c \
	gda-default-recordset.h \
	gda-default.h \
	main.c

gda_default_srv_2_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(SQLITE_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Default.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
