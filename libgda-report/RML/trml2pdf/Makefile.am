files = color.py \
	__init__.py  \
	trml2pdf.py \
	utils.py

install-exec-local:
	-mkdir -p $(DESTDIR)$(bindir)/gda_trml2pdf
	-cd $(top_srcdir)/libgda-report/RML/trml2pdf && $(INSTALL_PROGRAM) $(files) $(DESTDIR)$(bindir)/gda_trml2pdf

uninstall-local:
	-rm -rf $(DESTDIR)$(bindir)/gda_trml2pdf

EXTRA_DIST = $(files)
