SUBDIRS=libgda-common libgda-client

DOC_MODULE=libgda-reference

DOC_MAIN_SGML_FILE=libgda-reference.sgml

HTML_DIR=$(datadir)/gnome/html

TARGET_DIR=$(HTML_DIR)/$(DOC_MODULE)

content_files = \
	libgda-reference.sgml

local_entities = \
	libgda-common/libgda-common-docs.sgml \
	libgda-common/sgml/gda-common.sgml \
	libgda-common/sgml/gda-config.sgml \
	libgda-common/sgml/gda-corba.sgml \
	libgda-common/sgml/gda-log.sgml \
	libgda-common/sgml/gda-thread.sgml \
	libgda-common/sgml/gda-util.sgml \
	libgda-common/sgml/gda-xml-database.sgml \
	libgda-common/sgml/gda-xml-document.sgml \
	libgda-client/libgda-client-docs.sgml \
	libgda-client/sgml/GDA.sgml \
	libgda-client/sgml/gda-batch.sgml \
	libgda-client/sgml/gda-command.sgml \
	libgda-client/sgml/gda-connection-pool.sgml \
	libgda-client/sgml/gda-connection.sgml \
	libgda-client/sgml/gda-error.sgml \
	libgda-client/sgml/gda-field.sgml \
	libgda-client/sgml/gda-recordset.sgml \
	libgda-client/sgml/gda-value.sgml

EXTRA_DIST = $(content_files)

all: html/index.html

if ENABLE_GTK_DOC
html/index.html: $(content_files) $(local_entities)
	test -d $(srcdir)/html || mkdir $(srcdir)/html
	-cd $(srcdir)/html && gtkdoc-mkhtml $(DOC_MODULE) ../$(DOC_MAIN_SGML_FILE)
else
html/index.html:
endif

clean-local:
	rm -f *~ *.bak *.signals *-unused.txt

maintainer-clean-local:
	cd $(srcdir) && rm -rf html

if ENABLE_GTK_DOC
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(TARGET_DIR)
	(installfiles=`echo $(srcdir)/html/*.html`; \
	if test "$$installfiles" = '$(srcdir)/html/*.html'; \
	then echo '-- Nothing to install' ; \
	else \
	  for i in $$installfiles; do \
	    echo '-- Installing '$$i ; \
	    $(INSTALL_DATA) $$i $(DESTDIR)$(TARGET_DIR); \
	  done; \
	  echo '-- Installing $(srcdir)/html/index.sgml' ; \
	  $(INSTALL_DATA) $(srcdir)/html/index.sgml $(DESTDIR)$(TARGET_DIR); \
	  echo '-- Fixing Crossreferences' ; \
	  gtkdoc-fixxref --module=$(DOC_MODULE) --html-dir=$(HTML_DIR)|| true; \
	fi)
endif

dist-hook:
	mkdir $(distdir)/html
	-cp $(srcdir)/html/*.html $(srcdir)/html/*.css $(distdir)/html
