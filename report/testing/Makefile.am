INCLUDES = \
        -I$(top_builddir) \
        -I$(top_builddir)/report \
        -I$(top_srcdir) \
        $(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-report-test 

gda_report_test_SOURCES = \
        gda-report-test.c

gda_report_test_LDADD = \
        $(top_builddir)/libgda/libgda-2.la \
        $(top_builddir)/report/libgda-report/libgda-report-2.la \
        $(LIBGDA_LIBS)
