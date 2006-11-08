GAPI_SOURCE = gda-sources.xml
RAW_API = gda-api.raw
API = gda-api.xml
METADATA = Gda.metadata

ASSEMBLY_NAME = gda-sharp
ASSEMBLY = $(ASSEMBLY_NAME).dll

sources = Application.cs
build_sources = $(addprefix $(srcdir)/, $(sources)) AssemblyInfo.cs 

customs = 

CLEANFILES = gda-sharp.dll generated-stamp generated/*.cs $(API)

DISTCLEANFILES= AssemblyInfo.cs

pkglib_DATA = gda-sharp.dll gda-sharp.dll.config gda-api.xml

$(RAW_API): $(GAPI_SOURCE)
	$(GAPI_PARSER) $(GAPI_SOURCE)

$(API): $(RAW_API) $(METADATA)
	cp $(srcdir)/$(RAW_API) $(API)
	chmod u+w $(API)
	$(GAPI_FIXUP) --api=$(API) --metadata=$(srcdir)/$(METADATA)

generated-stamp: $(API)
	 $(GAPI_CODEGEN) --generate $(API) $(GTK_SHARP_CFLAGS) --outdir=generated --customdir=$(srcdir) --assembly-name=gda-sharp && touch generated-stamp

gda-sharp.dll: $(build_sources) generated-stamp
	$(MCS) -nowarn:169 -unsafe -target:library $(GTK_SHARP_LIBS) \
		$(build_sources) generated/*.cs -out:gda-sharp.dll

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gda-sharp-3.0.pc

EXTRA_DIST =			\
	$(GAPI_SOURCE)		\
	$(RAW_API)		\
	$(METADATA)		\
	$(customs)		\
	$(sources)		\
	AssemblyInfo.cs.in	\
	gda-sharp-3.0.pc.in	\
	gda-sharp.dll.config.in
