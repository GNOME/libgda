AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DCHECK_XML_FILES=\""$(top_srcdir)"\"

check_PROGRAMS = check_model_import check_virtual
TESTS = check_model_import check_virtual

common_sources = 

check_model_import_SOURCES = $(common_sources) check_model_import.c
check_model_import_CFLAGS = @CHECK_CFLAGS@
check_model_import_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)

check_virtual_SOURCES = $(common_sources) check_virtual.c
check_virtual_CFLAGS = @CHECK_CFLAGS@ \
	-I$(top_srcdir)/providers/sqlite \
	-I$(top_srcdir)/providers/sqlite/virtual
check_virtual_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/providers/sqlite/virtual/libgda-virtual-3.0.la \
	$(LIBGDA_LIBS)


EXTRA_DIST = \
	check_virtual.csv \
	city.csv \
	country.csv
