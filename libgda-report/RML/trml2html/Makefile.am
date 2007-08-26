files = __init__.py  \
	trml2html.py \
	utils.py

install-exec-local:
	-mkdir -p $(bindir)/gda_trml2html
	$(INSTALL_PROGRAM) $(files) $(bindir)/gda_trml2html

EXTRA_DIST = $(files)
