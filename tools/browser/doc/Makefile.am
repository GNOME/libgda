## Process this file with automake to produce Makefile.in

# The name of the module.
DOC_MODULE=gda-browser

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/tools/browser


# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=--verbose

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GDA_DISABLE_DEPRECATED"

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --ignore-files="binreloc"

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=

# Extra options to supply to gtkdoc-fixref. Normally not needed.
# e.g. FIXXREF_OPTIONS=--extra-dir=../gdk-pixbuf/html --extra-dir=../gdk/html
FIXXREF_OPTIONS=

# Used for dependencies. The docs will be rebuilt if any of these change.
# e.g. HFILE_GLOB=$(top_srcdir)/gtk/*.h
# e.g. CFILE_GLOB=$(top_srcdir)/gtk/*.c
HFILE_GLOB=
CFILE_GLOB=

# Extra header to include when scanning, which are not under DOC_SOURCE_DIR
# e.g. EXTRA_HFILES=$(top_srcdir}/contrib/extra.h
EXTRA_HFILES=

# Header files or dirs to ignore when scanning. Use base file/dir names
# e.g. IGNORE_HFILES=gtkdebug.h gtkintl.h private_code
IGNORE_HFILES= marshal.h canvas dummy-perspective


# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = fdl-appendix.sgml

# Images to copy into HTML directory
HTML_IMAGES = UILayout.png

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = -I$(top_srcdir) \
        -I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda-report \
	-I$(top_srcdir)/libgda \
	-I$(top_srcdir)/tools/browser \
	-I$(top_srcdir)/tools/browser/common \
        $(COREDEPS_CFLAGS) \
	$(COREDEPS_WFLAGS) \
        $(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"

ADDCANVASLIBS=
if HAVE_GOOCANVAS
ADDCANVASLIBS+=$(top_builddir)/tools/browser/canvas/libcanvas.la -lm
endif

ADDLDAPLIBS=
if LDAP
ADDLDAPLIBS+=$(top_builddir)/tools/browser/ldap-browser/libperspective.la
endif

GTKDOC_LIBS = \
	$(top_builddir)/tools/browser/data-manager/libperspective.la \
	$(top_builddir)/tools/browser/schema-browser/libperspective.la \
	$(ADDCANVASLIBS) \
	$(top_builddir)/tools/browser/query-exec/libperspective.la \
	$(ADDLDAPLIBS) \
	$(top_builddir)/tools/browser/libbrowser.la \
	$(top_builddir)/libgda-ui/internal/libgda-ui-internal.la \
	$(top_builddir)/tools/browser/common/libcommon.la \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-5.0.la \
	$(COREDEPS_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS)

include $(top_srcdir)/gtk-doc.make


# Comment this out if you don't want 'make check' to test you doc status
# and run some sanity checks
# if ENABLE_GTK_DOC
# TESTS_ENVIRONMENT = \
#   DOC_MODULE=$(DOC_MODULE) DOC_MAIN_SGML_FILE=$(DOC_MAIN_SGML_FILE) \
#   SRCDIR=$(abs_srcdir) BUILDDIR=$(abs_builddir)
# TESTS = $(GTKDOC_CHECK)
# endif


CLEANFILES += $(DOC_STAMPS)

