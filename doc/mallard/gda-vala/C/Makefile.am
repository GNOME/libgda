BUILT_SOURCES = gda.vala.stamp

HELP_FILES = \
	classes.page \
	enable-vala-extensions.page \
	index.page \
	interfaces.page \
	db-table-interface.page \
	db-field-info-interface.page \
	record-class.page \
	table-class.page

HELP_MEDIA = \
	figures/classes-uml.png \
	figures/record-class-uml.png \
	figures/interfaces-uml.png \
	figures/table-class-uml.png

htmldoc_SOURCES = \
	gda.vala.stamp \
	$(DOC_PAGES:.page=.html)

gda.vala.stamp: $(HELP_FILES)
	$(YELP_BUILD) html $^

JS_FILES = C.css jquery.js jquery.syntax.js jquery.syntax.brush.clang.js jquery.syntax.layout.yelp.js jquery.syntax.core.js yelp.js yelp-note-tip.png

C.css jquery.js jquery.syntax.js jquery.syntax.brush.clang.js jquery.syntax.layout.yelp.js jquery.syntax.core.js yelp.js yelp-note-tip.png: gda.vala.stamp

htmldocdir = \
	$(HTML_DIR)/libgda-5.0/gda-vala

htmldoc_DATA = \
	$(HELP_FILES:.page=.html) \
	$(JS_FILES)

figures: htmldoc

figuresdir = \
	$(HTML_DIR)/libgda-5.0/gda-vala/figures

figures_DATA = \
	$(HELP_MEDIA)

EXTRA_DIST = \
	$(HELP_MEDIA) \
	$(HELP_FILES) \
	$(JS_FILES)

CLEANFILES = \
	$(HELP_FILES:.page=.html) \
	$(JS_FILES)

clean-local:
	if test x${srcdir} != x$(builddir); then rm -rf $(builddir)/figures; fi
