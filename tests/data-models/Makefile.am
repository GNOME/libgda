AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DCHECK_XML_FILES=\""$(top_srcdir)"\"

check_PROGRAMS = check_model_import
TESTS = check_model_import

common_sources = 

check_model_import_SOURCES = $(common_sources) check_model_import.c
check_model_import_CFLAGS = @CHECK_CFLAGS@
check_model_import_LDADD = @CHECK_LIBS@ \
	$(top_builddir)/libgda/libgda-3.0.la \
	$(LIBGDA_LIBS)


EXTRA_DIST = 
