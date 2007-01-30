AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/report \
	-I$(top_builddir)/report \
	$(LIBGDA_CFLAGS)

bin_PROGRAMS = gda-report-test-3.0

gda_report_test_3_0_SOURCES = \
        gda-report-test.c

gda_report_test_3_0_LDADD = \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/libgda/graph/libgda_graph-3.0.la \
	$(top_builddir)/libgda/handlers/libgda_handlers-3.0.la \
	$(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la \
	$(top_builddir)/report/libgda-report/libgda-report-3.0.la \
	$(LIBGDA_LIBS)
