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
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-ldap-server/libgda-ldap.a \
	$(LDAP_LIBS)

libgda_ldap_a_SOURCES = \
	gda-ldap-command.c \
	gda-ldap-connection.c \
	gda-ldap-recordset.c \
	gda-ldap.h

oafdir=$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_LDAP.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=$(oaf_in_files) $(oaf_DATA)
