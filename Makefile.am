if ENABLE_GTK_DOC
TESTS=$(GTKDOC_CHECK)
endif

DISTCHECK_CONFIGURE_FLAGS = --enable-gi-system-install=no

if HAVE_LIBXSLT
XSLT_DIR=libgda-xslt
XSLT_PC=libgda-xslt-$(GDA_ABI_VERSION).pc
endif

if HAVE_UI
UI_DIR=libgda-ui
UI_PC=libgda-ui-$(GDA_ABI_VERSION).pc
endif

if ENABLE_TOOLS
TOOLS_DIR=tools
if HAVE_UI
UI_TOOLS_DIR=control-center
endif
endif

if ENABLE_EXAMPLES
EXAMPLES_DIR=examples
endif

if ENABLE_GTK_DOC
DOC_DIR=doc
endif

SUBDIRS = \
	po \
	libgda \
	providers \
	libgda-report \
	$(XSLT_DIR) \
	$(UI_DIR) \
	$(TOOLS_DIR) \
	$(UI_TOOLS_DIR) \
	tests \
	testing \
	data \
	$(EXAMPLES_DIR) \
	$(DOC_DIR)

example_files = \
	examples/SimpleExample/example.c \
	examples/SimpleExample/README \
	examples/TableCopy/common.h \
	examples/TableCopy/common.c \
	examples/TableCopy/table-copy.c \
	examples/TableCopy/table-copy-easier.c \
	examples/TableCopy/README \
	examples/BDB/README \
	examples/BDB/access-custom.c \
	examples/BDB/access-raw.c \
	examples/BDB/common.h \
	examples/BDB/custom-bdb-model.c \
	examples/BDB/custom-bdb-model.h \
	examples/BDB/fill.c \
	examples/DirDataModel/README \
	examples/DirDataModel/find-duplicates.c \
	examples/DDL/README \
	examples/DDL/ddl.c \
	examples/F-Spot/README \
	examples/F-Spot/VirtualConnectionSetup.dia \
	examples/F-Spot/repair-path.c \
	examples/README \
	examples/Report/customers-report.c \
	examples/Report/customers-report-docbook.c \
	examples/Report/customers-report-rml.c \
	examples/Report/customers-report-rml.rml \
	examples/Report/customers-report-spec.xml \
	examples/Report/schema-report.c \
	examples/Report/schema-report-spec.xml \
	examples/Report/README \
	examples/Virtual/README \
	examples/Virtual/city.csv \
	examples/Virtual/country.csv \
	examples/Virtual/virtual-test.c \
	examples/XSLT/README \
	examples/XSLT/data.xml \
	examples/XSLT/data2.xml \
	examples/XSLT/test.xsl \
	examples/XSLT/transform.c \
	examples/MetaStore/README \
	examples/MetaStore/background.c \
	examples/MetaStore/example.c \
	examples/Tree/README \
	examples/Tree/example.c \
	examples/SqlBuilder/README \
	examples/SqlBuilder/example.c \
	examples/AsyncExec/README \
	examples/AsyncExec/example.c \
	examples/WritableSelect/README \
	examples/WritableSelect/example.c \
	examples/SimpleUIForm/README \
	examples/SimpleUIForm/example.c \
	examples/SimpleUIForm/ScreenShot.png \
	examples/Blobs/blobtest.c \
	examples/Blobs/Makefile.cross.win32
	examples/Blobs/README \
	examples/Blobs/testblob.db \
	examples/Gir/README \
	examples/Gir/libgda.js \
	examples/LdapBrowser/README \
	examples/LdapBrowser/ldap-browser.c \
	examples/CustomUIPlugin/README \
	examples/CustomUIPlugin/custom-entry-password.c \
	examples/CustomUIPlugin/custom-entry-password.h \
	examples/CustomUIPlugin/custom-entry-password.xml \
	examples/CustomUIPlugin/libmain.c

# Meson build distribution
meson_build=\
	meson.build
	tlibgda-xslt/meson.build
	tcontrol-center/meson.build
	tproviders/reuseable/meson.build
	tproviders/reuseable/postgres/meson.build
	tproviders/reuseable/mysql/meson.build
	tproviders/meson.build
	tproviders/skel-implementation/meson.build
	tproviders/skel-implementation/models/meson.build
	tproviders/skel-implementation/capi/meson.build
	tproviders/sqlite/meson.build
	tproviders/ldap/meson.build
	tproviders/postgres/meson.build
	tproviders/jdbc/meson.build
	tproviders/bdb/meson.build
	tproviders/mysql/meson.build
	ttesting/meson.build
	tmeson.build
	tlibgda/binreloc/meson.build
	tlibgda/thread-wrapper/meson.build
	tlibgda/providers-support/meson.build
	tlibgda/meson.build
	tlibgda/sqlite/virtual/meson.build
	tlibgda/sqlite/meson.build
	tlibgda/sqlite/sqlite-src/meson.build
	tlibgda/handlers/meson.build
	tlibgda/sql-parser/meson.build
	tlibgda-ui/glade/meson.build
	tlibgda-ui/data/meson.build
	tlibgda-ui/meson.build
	tlibgda-ui/internal/meson.build
	tlibgda-ui/data-entries/meson.build
	tlibgda-ui/data-entries/plugins/meson.build
	tlibgda-ui/marshallers/meson.build
	ttools/common/meson.build
	ttools/meson.build
	ttools/base/meson.build
	ttools/browser/dummy-perspective/meson.build
	ttools/browser/data/meson.build
	ttools/browser/data/256x256/meson.build
	ttools/browser/data/48x48/meson.build
	ttools/browser/schema-browser/meson.build
	ttools/browser/query-exec/meson.build
	ttools/browser/data-manager/meson.build
	ttools/browser/meson.build
	ttools/browser/canvas/meson.build
	ttools/misc/meson.build
	ttools/help/meson.build
	ttools/help/C/meson.build
	ttools/webdata/meson.build
	tdoc/meson.build
	tdoc/C/meson.build
	tdoc/C/libgda/meson.build
	tdoc/C/libgdaui/meson.build
	tlibgda-report/RML/meson.build
	tlibgda-report/RML/trml2html/meson.build
	tlibgda-report/RML/trml2pdf/meson.build
	tlibgda-report/DocBook/meson.build
	tlibgda-report/engine/meson.build
	tlibgda-report/meson.build
	ttests/ui/meson.build
	ttests/providers/meson.build
	ttests/meson.build
	ttests/multi-threading/meson.build
	ttests/meta-store/meson.build
	ttests/data-models/meson.build
	ttests/parser/meson.build
	ttests/ddl/meson.build
	ttests/value-holders/meson.build

EXTRA_DIST = \
	COPYING \
	libgda.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	$(example_files) \
	getsp.class \
	getsp.java \
	tap-driver.sh \
	tap-test \
	$(meson_build)

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		( cd "$(top_srcdir)" && \
		  echo '# Generated by Makefile. Do not edit.'; echo; \
		  $(top_srcdir)/missing --run $(top_srcdir)/gitlog2changelog.py ) > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		     echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda-$(GDA_ABI_VERSION).pc libgda-report-$(GDA_ABI_VERSION).pc $(XSLT_PC) $(UI_PC)

all-local: libgda.spec

CLEANFILES = intltool-extract intltool-merge intltool-update

