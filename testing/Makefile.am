INCLUDES = \
	-I$(top_builddir) \
	-I$(top_srcdir) \
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
	sybase-test.h

gda_test_LDADD = \
	$(top_builddir)/libgda/libgda-2.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = sample_database.xml
