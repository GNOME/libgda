# The name of the module.
DOC_MODULE=libgda-common

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_builddir)/lib/gda-common

CFLAGS =" \
	-I$(top_builddir) \
	-I$(top_builddir)/lib/gda-common \
	$(GDA_CLIENT_CFLAGS) \
	"

LDFLAGS=" \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/.libs/libgda-common.a \
	"

DOC_DIR=$(datadir)/gnome/html

DOC_DIR_INSTALL_FILES = \
	 libgda-common.args \
	 libgda-common.hierarchy \
	 libgda-common.signals	\
	 libgda-common.types \
	 libgda-common-decl.txt		\
	 libgda-common-sections.txt

SOURCE_FILES = \
	$(top_builddir)/lib/gda-common/gda-common.h \
	$(top_builddir)/lib/gda-common/gda-config.c \
	$(top_builddir)/lib/gda-common/gda-config.h \
	$(top_builddir)/lib/gda-common/gda-corba.c \
	$(top_builddir)/lib/gda-common/gda-corba.h \
	$(top_builddir)/lib/gda-common/gda-init.c \
	$(top_builddir)/lib/gda-common/gda-log.c \
	$(top_builddir)/lib/gda-common/gda-log.h \
	$(top_builddir)/lib/gda-common/gda-thread.c \
	$(top_builddir)/lib/gda-common/gda-thread.h \
	$(top_builddir)/lib/gda-common/gda-util.c \
	$(top_builddir)/lib/gda-common/gda-util.h \
	$(top_builddir)/lib/gda-common/gda-xml-database.c \
	$(top_builddir)/lib/gda-common/gda-xml-database.h \
	$(top_builddir)/lib/gda-common/gda-xml-document.c \
	$(top_builddir)/lib/gda-common/gda-xml-document.h
 
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-atom.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-atom.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-bin.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-bin.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-column.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-column.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-const.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-const.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-delete.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-delete.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-dml.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-dml.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-dual.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-dual.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-field.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-field.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-func.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-func.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-insert.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-insert.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-item.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-item.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-join.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-join.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-list.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-list.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-query.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-query.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-select.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-select.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-stack.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-stack.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-target.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-target.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-update.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-update.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-value.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-value.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-valueref.c \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-valueref.h \
#	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query.h

IGNORED_HEADER_FILES = \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-atom-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-bin-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-column-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-const-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-delete-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-dml-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-dual-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-field-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-func-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-insert-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-item-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-join-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-list-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-query-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-select-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-stack-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-target-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-update-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-value-private.h \
	$(top_builddir)/lib/gda-common/gda-xml-query/xml-query-valueref-private.h

scan_generated = \
	libgda-common-decl.txt \
	libgda-common.args \
	libgda-common.hierarchy \
	libgda-common.signals \
	libgda-common.types

tmpl_dependencies = \
	libgda-common-decl.txt \
	libgda-common-sections.txt \
	libgda-common.args \
	libgda-common.hierarchy \
	libgda-common.signals

tmpl_sources = \
	tmpl/libgda-common.sgml	\
	tmpl/libgda-common-unused.sgml

tmpl_generated = \
	libgda-common-unused.txt

sgml_dependencies = \
	libgda-common-decl.txt \
	libgda-common-sections.txt \
	libgda-common.args \
	libgda-common.hierarchy \
	libgda-common.signals \
	tmpl/libgda-common.sgml

sgml_generated = \
	sgml/libgda-common.sgml \
	sgml/libgda-common-doc.bottom \
	sgml/libgda-common-doc.top	\
	sgml/object_index.sgml			\
	sgml/tree_index.sgml

EXTRA_DIST = \
	 libgda-common.args \
	 libgda-common.hierarchy	\
	 libgda-common.signals \
	 libgda-common.types \
	 libgda-common-decl.txt \
	 libgda-common-sections.txt \
	 libgda-common-docs.sgml

all: $(sgml_generated)

if ENABLE_GTK_DOC
scan $(scan_generated): $(SOURCE_FILES)
	-( env CFLAGS=$(CFLAGS) LDFLAGS=$(LDFLAGS)					\
		 gtkdoc-scanobj --module=$(DOC_MODULE)					\
	  && gtkdoc-scan --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)		\
			 --ignore-headers="$(IGNORED_HEADER_FILES)" )

templates $(tmpl_sources) $(tmpl_generated): $(tmpl_dependencies)
	cd $(srcdir) && gtkdoc-mktmpl --module=$(DOC_MODULE)

sgml $(sgml_generated): $(sgml_dependencies)
	cd $(srcdir) \
	&& gtkdoc-mkdb --module=$(DOC_MODULE) --source-dir=$(DOC_SOURCE_DIR)
else
scan $(scan_generated): $(SOURCE_FILES)

templates $(tmpl_sources) $(tmpl_generated): $(tmpl_dependencies)

sgml $(sgml_generated): $(sgml_dependencies)
endif

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local: clean
	cd $(srcdir) && rm -rf sgml $(DOC_MODULE)-decl-list.txt $(DOC_MODULE)-decl.txt

install-data-local:
	$(mkinstalldirs) $(DOC_DIR)
	for i in $(DOC_DIR_INSTALL_FILES); do		  \
		$(INSTALL_DATA) $(srcdir)/$$i $(DOC_DIR); \
	done

dist-hook:
	mkdir $(distdir)/sgml
	mkdir $(distdir)/tmpl
	-cp $(srcdir)/tmpl/*.sgml $(distdir)/tmpl
	-cp $(srcdir)/sgml/*.sgml $(distdir)/sgml
	-cp $(srcdir)/sgml/*.bottom $(srcdir)/sgml/*.top $(distdir)/sgml

.PHONY: scan templates sgml
