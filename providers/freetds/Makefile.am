providerdir=$(libdir)/libgda-$(GDA_MAJOR_VERSION).$(GDA_MINOR_VERSION)/providers
provider_LTLIBRARIES = libgda-freetds.la

AM_CPPFLAGS = \
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

libgda_freetds_la_LDFLAGS = -export-dynamic -module -avoid-version -no-undefined
libgda_freetds_la_LIBADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(LIBGDA_LIBS) \
	$(FREETDS_LIBS)

