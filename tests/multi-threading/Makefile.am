AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

TESTS_ENVIRONMENT = GDA_TOP_SRC_DIR="$(abs_top_srcdir)" GDA_TOP_BUILD_DIR="$(abs_top_builddir)"
TESTS = check_mutex check_parser check_cnc_lock check_threaded_cnc check_wrapper
check_PROGRAMS = check_mutex check_parser check_cnc_lock check_threaded_cnc check_wrapper

common_sources = \
        ../test-errors.h \
        ../test-errors.c \
	common.c \
	common.h

check_mutex_SOURCES = check_mutex.c
check_mutex_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS)

check_parser_SOURCES = check_parser.c
check_parser_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS)

check_cnc_lock_SOURCES = check_cnc_lock.c $(common_sources)
check_cnc_lock_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS)

check_threaded_cnc_SOURCES = check_threaded_cnc.c $(common_sources)
check_threaded_cnc_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS)

check_wrapper_SOURCES = check_wrapper.c dummy-object.c dummy-object.h
check_wrapper_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS)

EXTRA_DIST = testdb.sql

DISTCLEANFILES = testdb.db storedb.db
