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
	$(top_builddir)/lib/gda-common/.libs/libgda-common-2.a \
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
	$(top_builddir)/lib/gda-common/gda-common-defs.h \
	$(top_builddir)/lib/gda-common/gda-common.h \
	$(top_builddir)/lib/gda-common/gda-config.c \
	$(top_builddir)/lib/gda-common/gda-config.h \
	$(top_builddir)/lib/gda-common/gda-corba.c \
	$(top_builddir)/lib/gda-common/gda-corba.h \
	$(top_builddir)/lib/gda-common/gda-error.c \
	$(top_builddir)/lib/gda-common/gda-error.h \
	$(top_builddir)/lib/gda-common/gda-field.c \
	$(top_builddir)/lib/gda-common/gda-field.h \
	$(top_builddir)/lib/gda-common/gda-init.c \
	$(top_builddir)/lib/gda-common/gda-listener.c \
	$(top_builddir)/lib/gda-common/gda-listener.h \
	$(top_builddir)/lib/gda-common/gda-log.c \
	$(top_builddir)/lib/gda-common/gda-log.h \
	$(top_builddir)/lib/gda-common/gda-quark-list.c \
	$(top_builddir)/lib/gda-common/gda-quark-list.h \
	$(top_builddir)/lib/gda-common/gda-util.c \
	$(top_builddir)/lib/gda-common/gda-util.h \
	$(top_builddir)/lib/gda-common/gda-xml-atom-item.c \
	$(top_builddir)/lib/gda-common/gda-xml-atom-item.h \
	$(top_builddir)/lib/gda-common/gda-xml-bin-item.c \
	$(top_builddir)/lib/gda-common/gda-xml-bin-item.h \
	$(top_builddir)/lib/gda-common/gda-xml-database.c \
	$(top_builddir)/lib/gda-common/gda-xml-database.h \
	$(top_builddir)/lib/gda-common/gda-xml-document.c \
	$(top_builddir)/lib/gda-common/gda-xml-document.h \
	$(top_builddir)/lib/gda-common/gda-xml-item.c \
	$(top_builddir)/lib/gda-common/gda-xml-item.h \
	$(top_builddir)/lib/gda-common/gda-xml-list-item.c \
	$(top_builddir)/lib/gda-common/gda-xml-list-item.h \
	$(top_builddir)/lib/gda-common/gda-xml-util.c \
	$(top_builddir)/lib/gda-common/gda-xml-util.h
 
IGNORED_HEADER_FILES = 

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
