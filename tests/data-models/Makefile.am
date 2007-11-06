AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DCHECK_FILES=\""$(top_srcdir)"\"

check_PROGRAMS = check_model_import check_virtual check_data_proxy
TESTS = check_model_import check_virtual check_data_proxy

common_sources = 

check_model_import_SOURCES = $(common_sources) check_model_import.c
check_model_import_CFLAGS = @CHECK_CFLAGS@
check_model_import_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_virtual_SOURCES = $(common_sources) check_virtual.c
check_virtual_CFLAGS = @CHECK_CFLAGS@ \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_srcdir)/libgda/sqlite/virtual
check_virtual_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_data_proxy_SOURCES = $(common_sources) check_data_proxy.c
check_data_proxy_CFLAGS = @CHECK_CFLAGS@ 
check_data_proxy_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)


EXTRA_DIST = \
	check_virtual.csv \
	city.csv \
	country.csv
