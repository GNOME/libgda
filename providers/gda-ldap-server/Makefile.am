bin_PROGRAMS = gda-ldap-srv
noinst_LIBRARIES = libgda-ldap.a

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server \
	$(LDAP_CFLAGS)

gda_ldap_srv_SOURCES = main-ldap.c
gda_ldap_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common-2.la \
	$(top_builddir)/lib/gda-server/libgda-server-2.la \
	$(top_builddir)/providers/gda-ldap-server/libgda-ldap.a \
	$(LDAP_LIBS)

libgda_ldap_a_SOURCES = \
	gda-ldap-command.c \
	gda-ldap-connection.c \
	gda-ldap-recordset.c \
	gda-ldap.h

serverdir=$(libdir)/bonobo/servers

server_in_files = GNOME_Database_Provider_LDAP.server.in
server_DATA = $(server_in_files:.server.in=.server)

@XML_I18N_MERGE_SERVER_RULE@

EXTRA_DIST=$(server_in_files) $(server_DATA)
