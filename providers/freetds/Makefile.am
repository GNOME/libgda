providerdir=$(prefix)/lib/libgda/providers
provider_LTLIBRARIES = libgda-freetds.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(FREETDS_CFLAGS)

libgda_freetds_la_SOURCES = \
	gda-freetds-provider.c \
	gda-freetds-provider.h \
	gda-freetds-recordset.c \
	gda-freetds-recordset.h \
	gda-freetds-types.c \
	gda-freetds-types.h \
	gda-freetds-message.c \
	gda-freetds-message.h \
	gda-freetds.h \
	gda-tds-schemas.h \
	libmain.c \
	utils.c

libgda_freetds_la_LDFLAGS = -export-dynamic -module -avoid-version
libgda_freetds_la_LIBADD = \
	$(LIBGDA_LIBS) \
	$(top_builddir)/libgda/libgda-2.la \
	$(FREETDS_LIBS)

