include $(top_srcdir)/glib-tap.mk

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

test_programs = \
    check_ddl_creator

check_ddl_creator_SOURCES = check-ddl-creator.c
check_ddl_creator_LDADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS)
