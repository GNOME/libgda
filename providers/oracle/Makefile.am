bin_PROGRAMS = gda-oracle-srv-2

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(ORACLE_CFLAGS)

gda_oracle_srv_2_SOURCES = \
	main.c
gda_oracle_srv_2_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(ORACLE_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Oracle.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
