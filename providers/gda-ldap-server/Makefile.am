bin_PROGRAMS = gda-ldap-srv
lib_LTLIBRARIES = libgda-ldap.la

INCLUDES = \
	-I. \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-server

gda_ldap_srv_SOURCES = main-ldap.c
gda_ldap_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-interbase/libgda-ldap.la

libgda_ldap_SOURCES = \
	gda-ldap-command.c \
	gda-ldap-connection.c \
	gda-ldap-recordset.c \
	gda-ldap.h

EXTRA_DIST = gda-ldap.oafinfo

oafinfodir=$(GDA_oafinfodir)
oafinfo_DATA=gda-ldap.oafinfo
