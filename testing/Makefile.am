INCLUDES = \
	-I$(top_builddir)/libgda \
	-I$(top_srcdir)/libgda \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-test 

gda_test_SOURCES = \
	client.c \
	config.c \
	models.c \
	gda-test.c \
	gda-test.h \
	value.c
gda_test_LDADD = \
	$(top_builddir)/libgda/libgda-2.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = sample_database.xml
