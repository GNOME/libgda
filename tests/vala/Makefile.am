NULL = 

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/libgda/data \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(GEE_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

TESTS_ENVIRONMENT = 
TESTS = check_dataobject check_datamodeliterator check_table
check_PROGRAMS = check_dataobject check_datamodeliterator check_table

BUILT_SOURCES = \
    check_dataobject.vala.stamp \
    check_datamodeliterator.vala.stamp \
    check_table.vala.stamp \
    $(NULL)

CLEANFILES = \
    check_dataobject.vala.stamp \
    check_datamodeliterator.vala.stamp \
    check_table.vala.stamp \
    $(NULL)

VALAFLAGS = \
    --pkg gee-0.8 \
    --pkg gobject-2.0 \
    --pkg libxml-2.0 \
    --pkg libgda-5.0 \
    --pkg gdadata-5.0 \
    --includedir=$(top_builddir) \
    --vapidir=$(top_srcdir)/libgda \
    --vapidir=$(top_builddir)/libgda/data \
    --vapidir=$(VAPIDIR) \
    $(NULL)

check_dataobject_SOURCES = \
	CheckDataObject.vala \
	$(NULL)

check_datamodeliterator_SOURCES = \
	CheckDataModelIterator.vala \
	$(NULL)

check_table_SOURCES = \
	CheckTable.vala \
	$(NULL)

check_dataobject_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-5.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

check_datamodeliterator_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-5.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

check_table_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-5.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

CLEANFILES += \
    dataobject.db \
    datamodeliterator.db \
    table.db \
    $(NULL)

EXTRA_DIST = 
