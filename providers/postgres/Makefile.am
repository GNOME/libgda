bin_PROGRAMS = gda-postgres-srv-2

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(POSTGRES_CFLAGS)

gda_postgres_srv_2_SOURCES = \
	gda-postgres-provider.c \
	gda-postgres-provider.h \
	gda-postgres-recordset.c \
	gda-postgres-recordset.h \
	gda-postgres.h \
	main.c \
	utils.c
gda_postgres_srv_2_LDADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(POSTGRES_LIBS)

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Postgres.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
