noinst_LIBRARIES = libxmlquery.a

SUBDIRS = . testing

INCLUDES = \
	$(GTK_CFLAGS) $(XML_CFLAGS)\
	-I. \
	-DDTDINSTALLDIR=\""$(datadir)/gda/dtd"\"

gob_lst = \
	xml-query-item.gob \
	xml-query-list.gob \
	xml-query-atom.gob \
	xml-query-bin.gob \
	xml-query-column.gob \
	xml-query-const.gob \
	xml-query-target.gob \
	xml-query-dual.gob \
	xml-query-join.gob \
	xml-query-dml.gob \
	xml-query-value.gob \
	xml-query-field.gob \
	xml-query-func.gob \
	xml-query-delete.gob \
	xml-query-insert.gob \
	xml-query-query.gob \
	xml-query-select.gob \
	xml-query-stack.gob \
	xml-query-update.gob \
	xml-query-valueref.gob

gob_src = $(gob_lst:.gob=.c)
gob_hdr = $(gob_lst:.gob=.h) $(gob_lst:.gob=-private.h)

%.c %.h: %.gob
	$(GOB) $<

CLEANFILES = $(gob_src) $(gob_hdr)

xmlquery_headers = \
	xml-query.h \
	read.h \
	utils.h \
	$(gob_lst) \
	$(gob_hdr)

libxmlquery_a_SOURCES = \
	$(xmlquery_headers) \
	$(gob_src) \
	utils.c \
	read.c


DTD_FILES = gda-xml-query.dtd
dtddir=$(datadir)/gda/dtd
dtd_DATA=$(DTD_FILES)

EXTRA_DIST = $(DTD_FILES)
