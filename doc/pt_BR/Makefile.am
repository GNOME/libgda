SGML_FILES = \
	libgda.sgml

EXTRA_DIST=$(SGML_FILES) topic.dat

libgdaptbr_helpdir = $(datadir)/gnome/help/libgda/pt_BR

libgdaptbr_help_DATA =         \
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
	$(mkinstalldirs) $(DESTDIR)$(libgdaptbr_helpdir)/images
	-for file in $(srcdir)/libgda/*.html $(srcdir)/libgda/*.css; do \
	basefile=`basename $$file`; \
	$(INSTALL_DATA) $(srcdir)/$$file $(DESTDIR)$(libgdaptbr_helpdir)/$$basefile; \
	done

libgda.ps: libgda.sgml
	-db2ps $<

libgda.rtf: libgda.sgml
	-db2rtf $<

