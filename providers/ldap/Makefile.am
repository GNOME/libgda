providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-ldap.la

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(LDAP_CFLAGS)

libgda_ldap_la_SOURCES = \
	gda-ldap-provider.c \
	gda-ldap-provider.h \
	gda-ldap.h \
	gda-ldap-recordset.c \
	gda-ldap-recordset.h \
	libmain.c \
	utils.c
libgda_ldap_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_ldap_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(LDAP_LIBS)

