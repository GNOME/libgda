bin_PROGRAMS = gda-oracle-srv
noinst_LIBRARIES = libgda-oracle.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(ORACLE_CFLAGS)

gda_oracle_srv_SOURCES = main-oracle.c
gda_oracle_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-oracle-server/libgda-oracle.a \
	$(ORACLE_LIBS)

libgda_oracle_a_SOURCES = \
	gda-oracle-command.c \
	gda-oracle-connection.c \
	gda-oracle-recordset.c \
	gda-oracle.h

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_Oracle.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA) gda-oracle.spec.in
