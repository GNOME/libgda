providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-mysql.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(MYSQL_CFLAGS)

libgda_mysql_la_SOURCES = \
	gda-mysql-provider.c \
	gda-mysql-provider.h \
	gda-mysql-recordset.c \
	gda-mysql-recordset.h \
	gda-mysql.h \
	libmain.c \
	utils.c
libgda_mysql_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_mysql_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(MYSQL_LIBS)

