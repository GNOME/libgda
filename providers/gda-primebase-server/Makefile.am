bin_PROGRAMS = gda-primebase-srv
noinst_LIBRARIES = libgda-primebase.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(PRIMEBASE_CFLAGS)

gda_primebase_srv_SOURCES = main-primebase.c
gda_primebase_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-primebase-server/libgda-primebase.a \
	$(PRIMEBASE_LIBS)

libgda_primebase_a_SOURCES = \
	gda-primebase-command.c \
	gda-primebase-connection.c \
	gda-primebase-recordset.c \
	gda-primebase.h

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Primebase.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
