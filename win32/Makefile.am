#
# The .def files were manually generated as follows:
#
# echo "EXPORTS" > $lib.def
# nm $lib.{a,so} | grep ' T ' | sed 's/.* T _/\t/' | sort -bu >> $lib.def
#

EXTRA_DIST =				\
	dummy.la				\
	libgda_graph-3.0.def		\
	libgda_handlers-3.0.def

if PLATFORM_WIN32

BOOTSTRAP_LIBS =			\
	libgda_graph-3.0.la		\
	libgda_handlers-3.0.la

noinst_DATA = $(BOOTSTRAP_LIBS)

libgda_graph-3.0.la: dummy.la libgda_graph-3.0.def
	mkdir -p .libs
	dlltool --output-lib=.libs/libgda_graph-3.0.dll.a --dllname=$(SOPREFIX)gda_graph-3.0-3.dll --input-def=$(srcdir)/libgda_graph-3.0.def
	sed -e s!%DLL%!$(SOPREFIX)gda_graph-3.0-3! -e s!%LIB%!libgda_graph-3.0! -e s!%PFX%!$(prefix)! < $(srcdir)/dummy.la > $@

libgda_handlers-3.0.la: dummy.la libgda_handlers-3.0.def
	mkdir -p .libs
	dlltool --output-lib=.libs/libgda_handlers-3.0.dll.a --dllname=$(SOPREFIX)gda_handlers-3.0-3.dll --input-def=$(srcdir)/libgda_handlers-3.0.def
	sed -e s!%DLL%!$(SOPREFIX)gda_handlers-3.0-3! -e s!%LIB%!libgda_handlers-3.0! -e s!%PFX%!$(prefix)! < $(srcdir)/dummy.la > $@

# This is necessary for relinking during install
install-data-local: $(BOOTSTRAP_LIBS)
	$(mkinstalldirs) $(DESTDIR)$(libdir)
	for la in $(BOOTSTRAP_LIBS) ; do \
		implib=`echo $$la | sed -e s!\.la!.dll.a!` ; \
		$(INSTALL_DATA) .libs/$$implib $(DESTDIR)$(libdir) ; \
		sed -e s!installed=no!installed=yes! < $$la > $(DESTDIR)$(libdir)/$$la ; \
	done

CLEANFILES = $(BOOTSTRAP_LIBS)

endif
