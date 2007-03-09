AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS)

TESTS = check_delimiter
check_PROGRAMS = check_delimiter

check_delimiter_SOURCES = test-util.h test-util.c check_delimiter.c $(top_builddir)/libgda/sql-delimiter/gda-sql-delimiter.h
check_delimiter_CFLAGS = @CHECK_CFLAGS@
check_delimiter_LDADD = @CHECK_LIBS@ $(top_builddir)/libgda/sql-delimiter/libgda_sql_delimiter-3.0.la

#Define the path so that the test executable can find the test XML file even when srcdir!=builddir 
#I would prefer to use check_delimiter_LDFLAGS for this, but that does not seem to work. murrayc.
INCLUDES = -DPATH_TO_TEST_XML=\""$(srcdir)"\"


EXTRA_DIST = basic_sql.xml
