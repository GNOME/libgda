AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
	$(JSON_GLIB_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\"

TESTS_ENVIRONMENT = GDA_TOP_SRC_DIR="$(abs_top_srcdir)" GDA_TOP_BUILD_DIR="$(abs_top_builddir)"
TESTS = check_dataobject
check_PROGRAMS = check_dataobject

common_sources = common.c common.h

BUILT_SOURCES = check_dataobject.vala.stamp
CLEANFILES = check_dataobject.vala.stamp

VALAFLAGS = \
    --pkg gee-1.0 \
    --pkg gobject-2.0 \
    $(GDA_TOP_SRC_DIR)/libgda-5.0.vapi

check_dataobject_VALASOURCES = \
	CheckDataObject.vala
	
check_holder_SOURCES = \
	check_dataobject.vala.stamp \
	$(check_dataobject_VALASOURCES:.vala=.c)

check_holder_LDADD = \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(COREDEPS_LIBS)

EXTRA_DIST = 
