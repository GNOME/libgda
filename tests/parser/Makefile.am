AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/libgda \
	-I$(top_builddir) \
	$(LIBGDA_CFLAGS) \
	-DROOT_DIR=\""$(top_srcdir)"\" 

TESTS = check_parser check_validation check_normalization check_dml_comp
check_PROGRAMS = check_parser check_validation check_normalization check_dml_comp

check_parser_SOURCES = check_parser.c
check_parser_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_validation_SOURCES = check_validation.c
check_validation_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_normalization_SOURCES = check_normalization.c
check_normalization_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

check_dml_comp_SOURCES = check_dml_comp.c
check_dml_comp_LDADD = \
	$(top_builddir)/libgda/libgda-4.0.la \
	$(LIBGDA_LIBS)

EXTRA_DIST = testdata.xml testvalid.xml
