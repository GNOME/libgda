files = color.py \
	__init__.py  \
	trml2pdf.py \
	utils.py

install-exec-local:
	-mkdir -p $(bindir)/gda_trml2pdf
	$(INSTALL_PROGRAM) $(files) $(bindir)/gda_trml2pdf

EXTRA_DIST = $(files)