## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libgda-3.0

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GDA_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../..

# Used for dependencies.
HFILE_GLOB=
CFILE_GLOB=

# Header files to ignore when scanning
IGNORE_HFILES= config.h acconfig.h gda-intl.h gda-marshal.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = -I$(top_srcdir) \
        -I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda-report \
	-I$(top_srcdir)/libgda \
        $(LIBGDA_CFLAGS) \
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"
GTKDOC_LIBS =  $(top_builddir)/libgda/libgda-3.0.la \
	$(top_builddir)/libgda-report/libgda-report-3.0.la \
	$(LIBGDA_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --ignore-files="lexer.c"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = fdl-appendix.sgml 

# Images to copy into HTML directory
HTML_IMAGES = query01.png query02.png query03.png query04.png query05.png query06.png DataModels.png \
	architecture.png parts.png stmt-unknown.png stmt-select.png stmt-insert1.png stmt-insert2.png \
	stmt-update.png stmt-compound.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += examples/full_example.c installation.xml limitations.xml migration.xml query.xml query-fields.xml \
	server-operation.xml gettingstarted.xml virtual.xml virtual-notice.xml\
	query01.svg query02.svg query03.svg query04.svg query05.svg query06.svg DataModels.svg \
	architecture.svg parts.svg stmt-unknown.svg stmt-select.svg stmt-insert1.svg stmt-insert2.svg \
	stmt-update.svg stmt-compound.svg

