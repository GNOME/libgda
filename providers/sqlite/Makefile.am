SUBDIRS = sqlite

bin_PROGRAMS = gda-default-srv

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	-I$(top_srcdir)/providers/default/sqlite \
	-I$(top_builddir)/providers/default/sqlite

gda_default_srv_SOURCES = \
	gda-default-provider.c \
	gda-default-provider.h \
	gda-default.h \
	main.c
	
gda_default_srv_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(top_builddir)/providers/default/sqlite/libsqlite.a

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Default.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
