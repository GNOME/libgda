bin_PROGRAMS = gda-odbc-srv
noinst_LIBRARIES = libgda-odbc.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(ODBC_CFLAGS)

gda_odbc_srv_SOURCES = main-odbc.c
gda_odbc_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-odbc-server/libgda-odbc.a \
	$(ODBC_LIBS)

libgda_odbc_a_SOURCES = \
	gda-odbc-command.c \
	gda-odbc-connection.c \
	gda-odbc-recordset.c \
	gda-odbc.h

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_ODBC.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA) gda-odbc.spec.in
