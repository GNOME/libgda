YELP_EXTRA_HTML = \
	C.css \
	yelp-note-tip.png \
	jquery.js \
	jquery.syntax.brush.clang.js \
	jquery.syntax.core.js \
	jquery.syntax.js \
	jquery.syntax.layout.yelp.js

VALA_EXTENSIONS_DOC = \
	classes.page \
	enable-vala-extensions.page \
	index.page \
	interfaces.page \
	record-class.page \
	table-class.page

VALA_EXTENSIONS_DOC_PNG = \
	classes-uml.png \
	record-class-uml.png \
	interfaces-uml.png \
	table-class-uml.png

vala_extensions_doc_html_SOURCES = \
	$(VALA_EXTENSIONS_DOC:.page=.html)

C.css yelp-note-tip.png jquery.js jquery.syntax.brush.clang.js jquery.syntax.core.js jquery.syntax.js jquery.syntax.layout.yelp.js: valadoc

# Adding a new page in VALA_EXTENSIONS_DOC must be added here too as html
classes.html enable-vala-extensions.html index.html interfaces.html record-class.html table-class.html: valadoc

valadoc:
	yelp-build html ./

# Vala extensions doc will be installed in libgda gtk-doc module
valadocdir = $(HTML_DIR)/libgda-5.0/gda-data
valadoc_DATA = \
	$(YELP_EXTRA_HTML) \
	$(vala_extensions_doc_html_SOURCES) \
	$(VALA_EXTENSIONS_DOC_PNG)

CLEANFILES = \
	$(VALA_EXTENSIONS_DOC:.page=.html) \
	$(YELP_EXTRA_HTML)
