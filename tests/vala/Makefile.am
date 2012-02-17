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
    $(NULL)

check_dataobject_VALASOURCES = \
	CheckDataObject.vala \
	$(NULL)

check_datamodeliterator_VALASOURCES = \
	CheckDataModelIterator.vala \
	$(NULL)

check_table_VALASOURCES = \
	CheckTable.vala \
	$(NULL)

check_dataobject.vala.stamp: $(check_dataobject_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H check_dataobject.h $^
	@touch $@

check_datamodeliterator.vala.stamp: $(check_datamodeliterator_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H check_datamodeliterator.h $^
	@touch $@

check_table.vala.stamp: $(check_table_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H check_table.h $^
	@touch $@

check_dataobject_SOURCES = $(check_dataobject_VALASOURCES:.vala=.c) check_dataobject.h
check_dataobject_CFLAGS= \
    -I$(top_builddir) \
    -I$(top_builddir)/libgda/data \
    $(NULL)
check_dataobject_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-5.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

check_datamodeliterator_SOURCES = $(check_datamodeliterator_VALASOURCES:.vala=.c) check_datamodeliterator.h
check_datamodeliterator_CFLAGS= \
	-I$(top_builddir) \
	-I$(top_builddir)/libgda/data \
	$(NULL)
check_datamodeliterator_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-5.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)


check_table_SOURCES = $(check_table_VALASOURCES:.vala=.c) check_table.h
check_table_CFLAGS= \
    -I$(top_builddir) \
    -I$(top_builddir)/libgda/data \
    $(NULL)
check_table_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-5.0.la \
	$(COREDEPS_LIBS) \
	$(GEE_LIBS)

CLEANFILES += \
    $(check_dataobject_VALASOURCES:.vala=.c) \
    check_dataobject.h \
    $(check_datamodeliterator_VALASOURCES:.vala=.c) \
    check_datamodeliterator.h \
    $(check_table_VALASOURCES:.vala=.c) \
    check_table.h \
    dataobject.db \
    datamodeliterator.db \
    table.db \
    $(NULL)

EXTRA_DIST = 
