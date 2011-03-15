## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=gda-browser

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GDA_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../

# Used for dependencies.
HFILE_GLOB=
CFILE_GLOB=

# Header files to ignore when scanning
IGNORE_HFILES= marshal.h canvas dummy-perspective

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
        $(LIBGDA_CFLAGS) \
	$(LIBGDA_WFLAGS) \
        $(GTK_CFLAGS) \
	$(GTKSOURCEVIEW_CFLAGS) \
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"

ADDLIBS=
if HAVE_GOOCANVAS
ADDLIBS+=$(top_builddir)/tools/browser/canvas/libcanvas.la
endif

GTKDOC_LIBS = \
	$(top_builddir)/tools/browser/data-manager/libperspective.la \
	$(top_builddir)/tools/browser/schema-browser/libperspective.la \
	$(ADDLIBS) \
	$(top_builddir)/tools/browser/query-exec/libperspective.la \
	$(top_builddir)/tools/browser/libbrowser.la \
	$(top_builddir)/libgda-ui/internal/libgda-ui-internal.la \
	$(top_builddir)/tools/browser/common/libcommon.la \
	$(top_builddir)/libgda/libgda-5.0.la \
	$(top_builddir)/libgda-ui/libgda-ui-5.0.la \
	$(LIBGDA_LIBS) \
	$(GTK_LIBS) \
	$(GTKSOURCEVIEW_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --ignore-files="binreloc"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = fdl-appendix.sgml

# Images to copy into HTML directory
HTML_IMAGES = UILayout.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

include $(top_srcdir)/gtk-doc.make


# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
DISTCLEANFILES = $(DOC_MODULE)-doc.pdf

DOC_STAMPS += pdf-build.stamp
CLEANFILES += $(DOC_STAMPS)

# Create the PDF
PDF: pdf-build.stamp
pdf-build.stamp:
	if test -f $(srcdir)/$(DOC_MAIN_SGML_FILE); then \
		dblatex $(srcdir)/$(DOC_MAIN_SGML_FILE) -o $(DOC_MODULE)-doc.pdf ; \
	fi
	touch pdf-build.stamp
