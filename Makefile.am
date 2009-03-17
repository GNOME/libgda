DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

if HAVE_LIBXSLT
XSLT_DIR=libgda-xslt
endif

SUBDIRS = \
	po \
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
	samples/TableCopy/common.h \
	samples/TableCopy/common.c \
	samples/TableCopy/Makefile \
	samples/TableCopy/table-copy.c \
	samples/TableCopy/table-copy-easier.c \
	samples/TableCopy/README \
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
	samples/DDL/Makefile \
	samples/DDL/README \
	samples/DDL/ddl.c \
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
	samples/XSLT/Makefile \
	samples/XSLT/README \
	samples/XSLT/data.xml \
	samples/XSLT/data2.xml \
	samples/XSLT/test.xsl \
	samples/XSLT/transform.c \
	samples/MetaStore/Makefile \
	samples/MetaStore/README \
	samples/MetaStore/example.c \
	samples/Tree/Makefile \
	samples/Tree/README \
	samples/Tree/example.c

EXTRA_DIST = \
	COPYING \
	libgda.spec \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	mkinstalldirs \
	$(example_files) \
	getsp.class \
	getsp.java

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA = libgda-$(GDA_ABI_VERSION).pc libgda-report-$(GDA_ABI_VERSION).pc libgda-xslt-$(GDA_ABI_VERSION).pc

all-local: libgda.spec

CLEANFILES = intltool-extract intltool-merge intltool-update

