NULL = 
if ENABLE_VALA_EXTENSIONS

TESTS_ENVIRONMENT = 
TESTS = check_dataobject
check_PROGRAMS = check_dataobject

BUILT_SOURCES = check_dataobject.vala.stamp
CLEANFILES = check_dataobject.vala.stamp

VALAFLAGS = \
    --pkg gee-1.0 \
    --pkg gobject-2.0 \
    --pkg libxml-2.0 \
    --pkg libgda-5.0 \
    --pkg gdadata-1.0 \
    --includedir=$(top_builddir) \
    --vapidir=$(top_srcdir)/libgda \
    --vapidir=$(top_builddir)/libgda/data \
    $(NULL)

check_dataobject_VALASOURCES = \
	CheckDataObject.vala

check_dataobject check_dataobject.vala.stamp: $(check_dataobject_VALASOURCES)
	$(VALA_COMPILER) $(VALAFLAGS) -o check_dataobject $^
	@touch $@

EXTRA_DIST = 

endif
