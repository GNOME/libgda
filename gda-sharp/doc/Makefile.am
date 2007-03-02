if ENABLE_MONODOC
TARGETS = gda-sharp-docs.zip gda-sharp-docs.tree gda-sharp-docs.source
docsdir = $(prefix)/lib/monodoc/sources
docs_DATA = $(TARGETS)
else
TARGETS = 
docsdir = $(datadir)
docs_DATA =
endif

gda-sharp-docs.zip gda-sharp-docs.tree: en/index.xml
	$(MDASSEMBLER) --ecma en -o gda-sharp-docs

en/index.xml: ../gda-sharp.dll
	$(MONODOCER) -assembly:../gda-sharp.dll -path:en

CLEANFILES = gda-sharp-docs.zip gda-sharp-docs.tree

EXTRA_DIST = $(TARGETS)

dist-hook:
	mkdir -p $(distdir)/en
	cp en/*.xml $(distdir)/en/
	mkdir -p $(distdir)/en/Gda
	cp en/Gda/*.xml $(distdir)/en/Gda/
