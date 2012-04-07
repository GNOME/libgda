AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(GTK_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

TESTS = check_sets
check_PROGRAMS = check_sets

check_sets_SOURCES = common.c common.h check_sets.c
check_sets_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-5.0.la \
	$(COREDEPS_LIBS) \
	$(GTK_LIBS)
