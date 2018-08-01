
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

test_programs = \
    check_ddl_creator \
    check_ddl_base \
    check_ddl_column \
    check_ddl_view

check_ddl_creator_SOURCES = check-ddl-creator.c
check_ddl_creator_LDADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS)

check_ddl_base_SOURCES = check-ddl-base.c
check_ddl_base_LDADD = \
       $(top_builddir)/libgda/libgda-6.0.la \
       $(COREDEPS_LIBS)

check_ddl_column_SOURCES = check-ddl-column.c
check_ddl_column_LDADD = \
       $(top_builddir)/libgda/libgda-6.0.la \
       $(COREDEPS_LIBS)

check_ddl_view_SOURCES = check-ddl-view.c
check_ddl_view_LDADD = \
       $(top_builddir)/libgda/libgda-6.0.la \
       $(COREDEPS_LIBS)


include $(top_srcdir)/glib-tap.mk

EXTRA_DIST += column_test.xml \
	     ddl-db.xml \
	     fkey_test.xml \
	     view_test.xml

DISTCLEANFILES=ddl-test-out.xml ddl_test.db