# The name of the module.
DOC_MODULE=libgda-client

# The directory containing the source code (if it contains documentation).
DOC_SOURCE_DIR=$(top_builddir)/lib/gda-client

CFLAGS =" \
	-I$(top_builddir) \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-client \
	$(GDA_CLIENT_CFLAGS) \
	"

LDFLAGS=" \
	$(GDA_CLIENT_LIBS) \
	$(top_builddir)/lib/gda-common/.libs/libgda-common.so \
	$(top_builddir)/lib/gda-client/.libs/libgda-client.a \
	"

DOC_DIR=$(datadir)/gnome/html

DOC_DIR_INSTALL_FILES = \
	 libgda-client.args \
	 libgda-client.hierarchy \
	 libgda-client.signals	\
	 libgda-client.types \
	 libgda-client-decl.txt		\
	 libgda-client-sections.txt

SOURCE_FILES = \
	$(top_builddir)/lib/gda-client/gda-batch.c \
	$(top_builddir)/lib/gda-client/gda-batch.h \
	$(top_builddir)/lib/gda-client/gda-client.h \
	$(top_builddir)/lib/gda-client/gda-command.c \
	$(top_builddir)/lib/gda-client/gda-command.h \
	$(top_builddir)/lib/gda-client/gda-connection-pool.c \
	$(top_builddir)/lib/gda-client/gda-connection-pool.h \
	$(top_builddir)/lib/gda-client/gda-connection.c \
	$(top_builddir)/lib/gda-client/gda-connection.h \
	$(top_builddir)/lib/gda-client/gda-export.c \
	$(top_builddir)/lib/gda-client/gda-export.h \
	$(top_builddir)/lib/gda-client/gda-field.c \
	$(top_builddir)/lib/gda-client/gda-field.h \
	$(top_builddir)/lib/gda-client/gda-recordset.c \
	$(top_builddir)/lib/gda-client/gda-recordset.h \
	$(top_builddir)/lib/gda-client/gda-value.h

IGNORED_HEADER_FILES =

scan_generated = \
	libgda-client-decl.txt \
	libgda-client.args \
	libgda-client.hierarchy \
	libgda-client.signals \
	libgda-client.types

tmpl_dependencies = \
	libgda-client-decl.txt \
	libgda-client-sections.txt \
	libgda-client.args \
	libgda-client.hierarchy \
	libgda-client.signals

tmpl_sources = \
	tmpl/libgda-client.sgml	\
	tmpl/libgda-client-unused.sgml

tmpl_generated = \
	libgda-client-unused.txt

sgml_dependencies = \
	libgda-client-decl.txt \
	libgda-client-sections.txt \
	libgda-client.args \
	libgda-client.hierarchy \
	libgda-client.signals \
	tmpl/libgda-client.sgml

sgml_generated = \
	sgml/libgda-client.sgml \
	sgml/libgda-client-doc.bottom \
	sgml/libgda-client-doc.top	\
	sgml/object_index.sgml			\
	sgml/tree_index.sgml

EXTRA_DIST = \
	 libgda-client.args \
	 libgda-client.hierarchy	\
	 libgda-client.signals \
	 libgda-client.types \
	 libgda-client-decl.txt \
	 libgda-client-sections.txt \
	 libgda-client-docs.sgml

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
