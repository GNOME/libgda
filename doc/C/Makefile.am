## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libgda
HTML_DIR=$(datadir)/gtk-doc/html

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../libgda

# Used for dependencies.
HFILE_GLOB=
CFILE_GLOB=

# Header files to ignore when scanning
IGNORE_HFILES= config.h acconfig.h gda-intl.h gda-marshal.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = -I$(top_srcdir) \
        -I$(top_builddir) \
        $(LIBGDA_CFLAGS)
GTKDOC_LIBS =  $(top_builddir)/libgda/libgda-2.la \
        $(LIBGDA_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = fdl-appendix.sgml 

# Images to copy into HTML directory
HTML_IMAGES = 

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += examples/full_example.c

