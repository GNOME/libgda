SGML_FILES = \
	libgda.sgml \
	fdl-appendix.sgml

EXTRA_DIST=$(SGML_FILES) topic.dat

libgda_helpdir = $(datadir)/gnome/help/libgda/es

libgda_help_DATA =         \
	libgda.html        \
	topic.dat

## The - before the command means to ignore it if it fails.  that way
## people can still build the software without the docbook tools

all:

libgda.html: libgda/libgda.html
	-cp libgda/libgda.html .
	-cp libgda/libgda.html libgda/index.html

libgda/libgda.html: $(SGML_FILES)
	-db2html libgda.sgml

## when we make dist, we include the generated HTML so people don't
## have to have the docbook tools
dist-hook:
	mkdir $(distdir)/libgda
	-cp libgda/*.html libgda/*.css $(distdir)/libgda
	-cp libgda.html $(distdir)

install-data-local: libgda.html
	$(mkinstalldirs) $(libgda_helpdir)/images
	-for file in $(srcdir)/libgda/*.html $(srcdir)/libgda/*.css; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(libgda_helpdir)/$$basefile; \
	done

libgda.ps: libgda.sgml
	-db2ps $<

libgda.rtf: libgda.sgml
	-db2rtf $<


