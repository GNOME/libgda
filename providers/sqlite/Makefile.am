providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-sqlite.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(SQLITE_CFLAGS)

libgda_sqlite_la_SOURCES = \
	gda-sqlite-provider.c \
	gda-sqlite-provider.h \
	gda-sqlite-recordset.c \
	gda-sqlite-recordset.h \
	gda-sqlite.h \
	utils.c \
	libmain.c

libgda_sqlite_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_sqlite_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-3.la \
	$(SQLITE_LIBS)

