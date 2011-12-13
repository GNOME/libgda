NULL = 

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	$(GEE_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

BUILT_SOURCES = \
    sample_dataobject.vala.stamp \
    sample_datamodeliterator.vala.stamp \
    $(NULL)

CLEANFILES = \
    sample_dataobject.vala.stamp \
    sample_datamodeliterator.vala.stamp \
    $(NULL)

VALAFLAGS = \
    --pkg gee-0.8 \
    --pkg gobject-2.0 \
    --pkg libxml-2.0 \
    --pkg libgda-5.0 \
    --pkg gdadata-0.2 \
    --includedir=$(top_builddir) \
    --vapidir=$(top_srcdir)/libgda \
    --vapidir=$(top_builddir)/libgda/data \
    $(NULL)

sample_dataobject_VALASOURCES = \
	SampleDataObject.vala \
	$(NULL)

sample_datamodeliterator_VALASOURCES = \
	SampleDataModelIterator.vala \
	$(NULL)

sample_dataobject.vala.stamp: $(sample_dataobject_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H sample_dataobject.h $^
	@touch $@

sample_datamodeliterator.vala.stamp: $(sample_datamodeliterator_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H sample_datamodeliterator.h $^
	@touch $@

sample_dataobject_SOURCES = $(sample_dataobject_VALASOURCES:.vala=.c) sample_dataobject.h
sample_dataobject_CFLAGS= \
    -I$(top_builddir)/libgda/data \
    $(NULL)
sample_dataobject_LDADD = \
    $(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-0.2.la \
	$(COREDEPS_LIBS)

sample_datamodeliterator_SOURCES = $(sample_datamodeliterator_VALASOURCES:.vala=.c) sample_datamodeliterator.h
sample_datamodeliterator_CFLAGS= \
    -I$(top_builddir)/libgda/data \
    $(NULL)
sample_datamodeliterator_LDADD = \
    $(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-0.2.la \
	$(COREDEPS_LIBS)

CLEANFILES += \
    $(sample_dataobject_VALASOURCES:.vala=.c) \
    sample_dataobject.h \
    $(sample_datamodeliterator_VALASOURCES:.vala=.c) \
    sample_datamodeliterator.h \
    dataobject.db \
    datamodeliterator.db \
    $(NULL)

EXTRA_DIST = 
