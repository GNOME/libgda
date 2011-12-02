NULL = 

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

TESTS_ENVIRONMENT = 
TESTS = check_dataobject
check_PROGRAMS = check_dataobject

BUILT_SOURCES = check_dataobject.vala.stamp
CLEANFILES = check_dataobject.vala.stamp

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

check_dataobject_VALASOURCES = \
	CheckDataObject.vala \
	$(NULL)

check_dataobject.vala.stamp: $(check_dataobject_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -C -H check_dataobject.h $^
	@touch $@

check_dataobject_SOURCES = $(check_dataobject_VALASOURCES:.vala=.c) check_dataobject.h
check_dataobject_LDADD = \
    $(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda/data/libgdadata-1.0.la \
	$(COREDEPS_LIBS)

EXTRA_DIST = 
