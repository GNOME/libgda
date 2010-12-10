AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\" \
	-DTOP_BUILD_DIR=\""$(top_builddir)"\"

TESTS = check_mutex check_parser check_cnc_lock check_threaded_cnc check_wrapper
check_PROGRAMS = check_mutex check_parser check_cnc_lock check_threaded_cnc check_wrapper

check_mutex_SOURCES = check_mutex.c
check_mutex_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_parser_SOURCES = check_parser.c
check_parser_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_cnc_lock_SOURCES = check_cnc_lock.c common.c common.h
check_cnc_lock_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_threaded_cnc_SOURCES = check_threaded_cnc.c common.c common.h
check_threaded_cnc_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_wrapper_SOURCES = check_wrapper.c dummy-object.c dummy-object.h
check_wrapper_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = testdb.sql

DISTCLEANFILES = testdb.db storedb.db
