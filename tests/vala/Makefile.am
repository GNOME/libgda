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

TESTS_ENVIRONMENT = GDA_TOP_SRC_DIR="$(abs_top_srcdir)" GDA_TOP_BUILD_DIR="$(abs_top_builddir)"
TESTS = check_dataobject check_table
check_PROGRAMS = check_dataobject check_table

CLEANFILES =

VALAFLAGS = \
    --pkg gee-0.8 \
    --pkg gobject-2.0 \
    --pkg libxml-2.0 \
    --pkg libgda-6.0 \
    --pkg gdadata-6.0 \
    --includedir=$(top_builddir) \
    --vapidir=$(top_srcdir)/libgda \
    --vapidir=$(top_builddir)/libgda/data \
    --vapidir=$(VAPIDIR)

check_dataobject_SOURCES = \
	CheckDataObject.vala

check_datamodeliterator_SOURCES = \
	CheckDataModelIterator.vala

check_table_SOURCES = \
	CheckTable.vala

check_dataobject_LDADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/libgda/data/libgdadata-6.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

check_datamodeliterator_LDADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/libgda/data/libgdadata-6.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

check_table_LDADD = \
	$(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/libgda/data/libgdadata-6.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

CLEANFILES += \
    dataobject.db \
    datamodeliterator.db \
    table.db

