
AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

test_programs = \
    check_db_catalog \
    check_db_base \
    check_db_column \
    check_db_view \
    check_db_catalog_postgresql

check_db_catalog_SOURCES = check-db-catalog.c
check_db_catalog_LDADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(COREDEPS_LIBS)

check_db_base_SOURCES = check-db-base.c
check_db_base_LDADD = \
       $(top_builddir)/libgda/libgda-6.0.la \
       $(COREDEPS_LIBS)

check_db_column_SOURCES = check-db-column.c
check_db_column_LDADD = \
       $(top_builddir)/libgda/libgda-6.0.la \
       $(COREDEPS_LIBS)

check_db_view_SOURCES = check-db-view.c
check_db_view_LDADD = \
       $(top_builddir)/libgda/libgda-6.0.la \
       $(COREDEPS_LIBS)

check_db_catalog_postgresql_SOURCES = check-db-catalog-postgresql.c
check_db_catalog_postgresql_LDADD = \
       $(top_builddir)/libgda/libgda-6.0.la \
       $(COREDEPS_LIBS)

include $(top_srcdir)/glib-tap.mk

EXTRA_DIST += column_test.xml \
	     db-db.xml \
	     fkey_test.xml \
	     view_test.xml

DISTCLEANFILES=db-test-out.xml db_test.db
