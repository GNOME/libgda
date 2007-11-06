AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DCHECK_XML_FILES=\""$(top_srcdir)"\" \
	-DCHECK_DICT_FILES=\""$(top_builddir)"\"

TESTS = check_delimiter check_gdaquery
check_PROGRAMS = check_delimiter check_gdaquery

check_delimiter_SOURCES = test-util.h test-util.c check_delimiter.c $(top_srcdir)/libgda/sql-delimiter/gda-sql-delimiter.h
check_delimiter_CFLAGS = @CHECK_CFLAGS@
check_delimiter_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_gdaquery_SOURCES = test-util.h test-util.c check_gdaquery.c $(top_srcdir)/libsql/sql_parser.h
check_gdaquery_CFLAGS = @CHECK_CFLAGS@
check_gdaquery_LDADD = @CHECK_LIBS@  \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = all_sql_tests.xml \
	app_sql.xml \
	basic_sql.xml \
	params_sql.xml \
	params_sql_simple.xml \
	prov_sql.xml \
	select_sql.xml \
	dict_pg.xml.in \
	bug_param_sql.xml \
	bug_param_sql_dict.xml.in
