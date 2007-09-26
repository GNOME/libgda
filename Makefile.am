DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

if HAVE_LIBXSLT
XSLT_DIR=libgda-xslt
endif

SUBDIRS = \
	po \
	libsql \
	libgda \
	providers \
	libgda-report \
	$(XSLT_DIR) \
	tools \
	tests \
	testing \
	data \
	doc

example_files = \
	samples/SimpleExample/Makefile \
	samples/SimpleExample/example.c \
	samples/SimpleExample/README \
	samples/TableCopy/Makefile \
	samples/TableCopy/table-copy.c \
	samples/TableCopy/table-copy-easier.c \
	samples/TableCopy/README \
	samples/README \
	samples/BDB/Makefile \
	samples/BDB/README \
	samples/BDB/access-custom.c \
	samples/BDB/access-raw.c \
	samples/BDB/common.h \
	samples/BDB/custom-bdb-model.c \
	samples/BDB/custom-bdb-model.h \
	samples/BDB/fill.c \
	samples/DirDataModel/Makefile \
	samples/DirDataModel/README \
	samples/DirDataModel/find-duplicates.c \
	samples/F-Spot/Makefile \
	samples/F-Spot/README \
	samples/F-Spot/VirtualConnectionSetup.dia \
	samples/F-Spot/repair-path.c \
	samples/README \
	samples/Report/customers-report.c \
	samples/Report/customers-report-docbook.c \
	samples/Report/customers-report-rml.c \
	samples/Report/customers-report-rml.rml \
	samples/Report/customers-report-spec.xml \
	samples/Report/Makefile
	samples/Report/README
	samples/Virtual/Makefile \
	samples/Virtual/README \
	samples/Virtual/city.csv \
	samples/Virtual/country.csv \
	samples/Virtual/virtual-test.c \
	samples/sample-firebird.c \
	samples/sample-postgresql.c \
	samples/table_copy.c 

EXTRA_DIST = \
	COPYING \
	libgda.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	mkinstalldirs \
	$(example_files)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda-3.0.pc libgda-report-3.0.pc libgda-xslt-3.0.pc

all-local: libgda.spec

CLEANFILES = intltool-extract intltool-merge intltool-update

