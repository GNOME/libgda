bin_PROGRAMS = gda-sybase-srv

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(SYBASE_CFLAGS)

gda_sybase_srv_SOURCES = \
	gda-sybase-provider.c \
	gda-sybase-provider.h \
	gda-sybase.h \
	gda-sybase-error.c \
	main.c

gda_sybase_srv_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(SYBASE_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Sybase.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
