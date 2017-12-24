## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libgda-6.0

# The top-level SGML file.
DOC_MAIN_SGML_FILE=$(DOC_MODULE)-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="GDA_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
SCANGOBJ_OPTIONS=

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/doc/src-files

# Add the builddir to the HTML path so that version.xml is found
MKHTML_OPTIONS=--path="$(abs_srcdir)"

# Used for dependencies.
HFILE_GLOB=
CFILE_GLOB=

# Header files to ignore when scanning
IGNORE_HFILES= gda-marshal.h gda-custom-marshal.h gda-enum-types.h sqlite3.h csv.h md5.h \
	jni-wrapper.h gda-jdbc-recordset.h gda-jdbc-blob-op.h gda-jdbc-provider.h gda-jdbc-pstmt.h \
	popup-container.h providers tools control-center tests examples plugins tmp

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
GTKDOC_CFLAGS = -I$(top_srcdir) \
        -I$(top_builddir) \
	-I$(top_srcdir)/libgda/sqlite \
	-I$(top_builddir)/libgda/sqlite \
	-I$(top_srcdir)/libgda-report \
	-I$(top_srcdir)/libgda \
        $(COREDEPS_CFLAGS) \
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"

GTKDOC_LIBS =  $(top_builddir)/libgda/libgda-6.0.la \
	$(top_builddir)/libgda-report/libgda-report-6.0.la \
	$(COREDEPS_LIBS)

if HAVE_UI
GTKDOC_CFLAGS += $(GTK_CFLAGS)
GTKDOC_LIBS += $(GTK_LIBS) \
	$(top_builddir)/libgda-ui/libgda-ui-6.0.la
endif

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml --name-space=gda --ignore-files="parser.c delimiter.c binreloc.c libcsv.c md5c.c providers tools control-center tests tmp examples plugins"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = fdl-appendix.sgml version.xml builddate.xml

# Images to copy into HTML directory
HTML_IMAGES = DataModels.png \
	architecture.png parts.png stmt-unknown.png stmt-select.png stmt-insert1.png stmt-insert2.png \
	stmt-update.png stmt-compound.png information_schema.png \
	MetaStore1.png MetaStore2.png i_s_data_types.png \
	writable_data_model.png GdaDataModelIter.png \
	data_validation_holder.png data_validation_proxy.png data_validation_set.png \
	data_proxy1.png data_proxy2.png data_proxy3.png data_proxy4.png data_proxy5.png \
	gda-sql-graph.png howto-exec.png GdaStatement.png \
	parser_gen.png parser_prov.png \
	tree-overview.png tree-overview2.png \
	SqlIdentifiers.png thread-wrapper.png \
	vi-basic-form.png vi-combo.png vi-data-entry.png \
	vi-login.png vi-cloud.png vi-provider-selector.png vi-raw-grid.png \
	vi-info.png vi-filter.png vi-server-op.png vi-rte.png \
	connection-status.png

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += examples/full_example.c installation.xml limitations.xml migration.xml migration2.xml migration3.xml migration4.xml \
	server-operation.xml gettingstarted.xml abstraction.xml virtual.xml virtual-notice.xml store-meta-type.xml \
	prov-writing-virtual-methods.xml prov-writing-recordsets.xml prov-writing-blobs.xml prov-writing-parser.xml prov-writing-assembly.xml \
	packaging.xml packaging_ui.xml i_s_doc.xml howto.xml gda-sql-manual.xml data_validation.xml data_select.xml \
	DataModels.svg \
	architecture.svg parts.svg stmt-unknown.svg stmt-select.svg stmt-insert1.svg stmt-insert2.svg \
	stmt-update.svg stmt-compound.svg information_schema.svg howto-exec.svg thread-wrapper.svg \
	version.xml.in builddate.xml.in \
	visual_index.xml prov-notes.xml \
	libgda-sections.txt libgda-ui-sections.txt libgda.types libgda.types.in libgda-ui.types \
	examples/blobtest.c \
	data-model-writing.xml

# Files not to distribute
# for --rebuild-types in $(SCAN_OPTIONS), e.g. $(DOC_MODULE).types
# for --rebuild-sections in $(SCAN_OPTIONS) e.g. $(DOC_MODULE)-sections.txt
DISTCLEANFILES = $(DOC_MODULE)-doc.pdf libgda-6.0.types libgda-6.0-sections.txt

DOC_STAMPS += pdf-build.stamp
CLEANFILES += $(DOC_STAMPS)

# Create the PDF
PDF: pdf-build.stamp
pdf-build.stamp:
	if test -f $(srcdir)/$(DOC_MAIN_SGML_FILE); then \
		dblatex $(srcdir)/$(DOC_MAIN_SGML_FILE) -o $(DOC_MODULE)-doc.pdf ; \
	fi
	touch pdf-build.stamp
