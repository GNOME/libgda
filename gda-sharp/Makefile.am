SUBDIRS = . doc

if BUILD_GDA_SHARP
pkg = gda
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gda-sharp-3.0.pc
else
pkg = 
endif

SYMBOLS = gda-sources.xml
METADATA = Gda.metadata

references = 
sources = Application.cs
customs = 

add_dist = gda-sharp-3.0.pc.in

SNK = gda-sharp.snk
API = $(pkg)-api.xml
RAW_API = $(pkg)-api.raw

ASSEMBLY_NAME = $(pkg)-sharp
ASSEMBLY = $(ASSEMBLY_NAME).dll

TARGET = $(pkg:=-sharp.dll) $(pkg:=-sharp.dll.config) $(POLICY_ASSEMBLIES)
noinst_DATA = $(TARGET)
TARGET_API = $(pkg:=-api.xml)
gapidir = $(datadir)/gapi-2.0
gapi_DATA = $(TARGET_API)
CLEANFILES = $(ASSEMBLY) $(ASSEMBLY).mdb $(POLICY_ASSEMBLIES) generated-stamp generated/*.cs $(API) $(SNK) AssemblyInfo.cs $(POLICY_CONFIGS)
DISTCLEANFILES = $(ASSEMBLY).config

# POLICY_ASSEMBLIES = $(addsuffix .$(ASSEMBLY), $(addprefix policy., $(POLICY_VERSIONS)))
# POLICY_CONFIGS = $(addsuffix .config, $(addprefix policy., $(POLICY_VERSIONS)))

EXTRA_DIST = $(RAW_API) $(SYMBOLS) $(ASSEMBLY).config.in $(METADATA) $(customs) $(sources) $(add_dist)

build_symbols = $(addprefix --symbols=$(srcdir)/, $(SYMBOLS))

$(srcdir)/$(RAW_API): $(SYMBOLS)
	@cd $(srcdir)
	$(GAPI_PARSER) $(SYMBOLS)

$(API): $(METADATA) $(RAW_API) $(SYMBOLS)
	cp $(srcdir)/$(RAW_API) $(API)
	chmod u+w $(API)
	@if test -n '$(METADATA)'; then		\
		echo "$(GAPI_FIXUP) --api=$(API) --metadata=$(srcdir)/$(METADATA)";	\
		$(GAPI_FIXUP) --api=$(API) --metadata=$(srcdir)/$(METADATA); \
	fi

build_customs = $(addprefix $(srcdir)/, $(customs))
api_includes = $(GLIB_SHARP_CFLAGS) $(addprefix -I:, $(INCLUDE_API))

generated-stamp: $(API) $(INCLUDE_API) $(build_customs)
	rm -f generated/* &&	\
	$(GAPI_CODEGEN) --generate $(API) \
	$(api_includes)	\
	--outdir=generated --customdir=$(srcdir) --assembly-name=$(ASSEMBLY_NAME)	\
	&& touch generated-stamp

$(SNK): $(top_srcdir)/$(SNK)
	cp $(top_srcdir)/$(SNK) .

$(POLICY_ASSEMBLIES): $(top_builddir)/policy.config $(SNK)
	@for i in $(POLICY_VERSIONS); do	\
	  echo "Creating policy.$$i.$(ASSEMBLY)";	\
	  sed -e "s/@ASSEMBLY_NAME@/$(ASSEMBLY_NAME)/" -e "s/@POLICY@/$$i/" $(top_builddir)/policy.config > policy.$$i.config;	\
	  $(AL) -link:policy.$$i.config -out:policy.$$i.$(ASSEMBLY) -keyfile:$(SNK);	\
	done

build_sources = $(addprefix $(srcdir)/, $(sources)) AssemblyInfo.cs
build_references = $(GLIB_SHARP_LIBS) $(addprefix /r:, $(references)) $(MONO_CAIRO_LIBS)

$(ASSEMBLY): generated-stamp $(SNK) $(build_sources) $(references)
	@rm -f $(ASSEMBLY).mdb
	$(MCS) $(CSFLAGS) -nowarn:0169,0612,0618 -unsafe -out:$(ASSEMBLY) -target:library $(build_references) $(GENERATED_SOURCES) $(build_sources)

install-data-local:
	@if test -n '$(pkg)'; then							\
	  echo "$(GACUTIL) /i $(ASSEMBLY) /f $(GACUTIL_FLAGS)";				\
	  $(GACUTIL) /i $(ASSEMBLY) /f $(GACUTIL_FLAGS) || exit 1;			\
	  if test -n '$(POLICY_VERSIONS)'; then						\
	    for i in $(POLICY_VERSIONS); do						\
	      echo "$(GACUTIL) /i policy.$$i.$(ASSEMBLY) /f $(GACUTIL_FLAGS)";		\
	      $(GACUTIL) /i policy.$$i.$(ASSEMBLY) /f $(GACUTIL_FLAGS) || exit 1;	\
	    done									\
	  fi										\
	fi

uninstall-local:
	@if test -n '$(pkg)'; then							\
	  echo "$(GACUTIL) /u $(ASSEMBLY_NAME) $(GACUTIL_FLAGS)";			\
	  $(GACUTIL) /u $(ASSEMBLY_NAME) $(GACUTIL_FLAGS) || exit 1;			\
	  if test -n '$(POLICY_VERSIONS)'; then						\
	    for i in $(POLICY_VERSIONS); do						\
	      echo "$(GACUTIL) /u policy.$$i.$(ASSEMBLY_NAME) $(GACUTIL_FLAGS)";	\
	      $(GACUTIL) /u policy.$$i.$(ASSEMBLY_NAME) $(GACUTIL_FLAGS) || exit 1;	\
	    done									\
	  fi										\
	fi
