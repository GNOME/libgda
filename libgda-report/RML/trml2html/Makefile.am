files = __init__.py  \
	trml2html.py \
	utils.py

install-exec-local:
	-mkdir -p $(DESTDIR)$(bindir)/gda_trml2html
	-cd $(top_srcdir)/libgda-report/RML/trml2html && $(INSTALL_PROGRAM) $(files) $(DESTDIR)$(bindir)/gda_trml2html

uninstall-local:
	-rm -rf $(DESTDIR)$(bindir)/gda_trml2html

EXTRA_DIST = $(files)
