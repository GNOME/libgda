AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-test 

gda_test_SOURCES = \
	client.c \
	config.c \
	models.c \
	models.h \
	gda-test.c \
	gda-test.h \
	value.c \
	postgres-test.c \
	postgres-test.h \
	freetds-test.c \
	freetds-test.h \
	sybase-test.c \
	sybase-test.h \
	ibmdb2-test.c \
	ibmdb2-test.h

gda_test_LDADD = \
	$(top_builddir)/libgda/libgda-3.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = sample_database.xml
