providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-mysql.la

INCLUDES = \
	-I. \
	$(LIBGDA_CFLAGS) \
	$(MYSQL_CFLAGS)

libgda_mysql_la_SOURCES = \
	gda-mysql-provider.c \
	gda-mysql-provider.h \
	gda-mysql-recordset.c \
	gda-mysql-recordset.h \
	gda-mysql.h \
	utils.c
libgda_mysql_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_mysql_la_LBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(MYSQL_LIBS)

