bin_PROGRAMS = gda-default-srv
noinst_LIBRARIES = libgda-default.a

INCLUDES = \
	$(GDA_PROVIDER_CFLAGS) \
	-I$(top_srcdir)/lib/gda-common \
	-I$(top_builddir)/lib/gda-common \
	-I$(top_srcdir)/lib/gda-server \
	-I$(top_builddir)/lib/gda-server \
	-I$(top_srcdir)/providers/gda-default-server \
	-I$(top_builddir)/providers/gda-default-server

gda_default_srv_SOURCES = main-default.c
gda_default_srv_LDADD = \
	$(GDA_PROVIDER_LIBS) \
	$(top_builddir)/lib/gda-common/libgda-common.la \
	$(top_builddir)/lib/gda-server/libgda-server.la \
	$(top_builddir)/providers/gda-default-server/libgda-default.a \
	$(top_builddir)/providers/gda-default-server/libsqlite.a \
	-lgdbm

libgda_default_a_SOURCES = \
	gda-default-command.c \
	gda-default-connection.c \
	gda-default-recordset.c \
	gda-default.h

sqlite.h: $(top_srcdir)/providers/gda-default-server/libsqlite.a

$(top_srcdir)/providers/gda-default-server/libsqlite.a: 
	mkdir -p $(top_srcdir)/providers/gda-default-server/build_sqlite && \
	cd $(top_srcdir)/providers/gda-default-server/build_sqlite && \
	../sqlite/configure && \
	make && \
	cp $(top_srcdir)/providers/gda-default-server/build_sqlite/libsqlite.a $(top_srcdir)/providers/gda-default-server/libsqlite.a && \
	cd $(top_srcdir)/providers/gda-default-server

oafdir=$(DESTDIR)$(GDA_oafdir)

oaf_in_files = GNOME_GDA_Provider_Default.oaf.in
oaf_DATA = $(oaf_in_files:.oaf.in=.oaf)

@XML_I18N_MERGE_OAF_RULE@

EXTRA_DIST=\
	$(oaf_in_files) \
	$(oaf_DATA) \
	sqlite/COPYRIGHT \
	sqlite/Makefile.in \
	sqlite/NO-AUTO-GEN \
	sqlite/README \
	sqlite/VERSION \
	sqlite/configure \
	sqlite/configure.in \
	sqlite/doc/lemon.html \
	sqlite/doc/report1.txt \
	sqlite/src/TODO \
	sqlite/src/build.c \
	sqlite/src/dbbe.c \
	sqlite/src/dbbe.h \
	sqlite/src/dbbegdbm.c \
	sqlite/src/dbbemem.c \
	sqlite/src/delete.c \
	sqlite/src/expr.c \
	sqlite/src/insert.c \
	sqlite/src/main.c \
	sqlite/src/notes1.txt \
	sqlite/src/notes2.txt \
	sqlite/src/pager.c \
	sqlite/src/pager.h \
	sqlite/src/parse.y \
	sqlite/src/printf.c \
	sqlite/src/printftest.c \
	sqlite/src/random.c \
	sqlite/src/select.c \
	sqlite/src/shell.c \
	sqlite/src/shell.tcl \
	sqlite/src/sqlite.h.in \
	sqlite/src/sqliteInt.h \
	sqlite/src/table.c \
	sqlite/src/tclsqlite.c \
	sqlite/src/tokenize.c \
	sqlite/src/update.c \
	sqlite/src/util.c \
	sqlite/src/vdbe.c \
	sqlite/src/vdbe.h \
	sqlite/src/where.c \
	sqlite/src/ex/README \
	sqlite/src/ex/db.c \
	sqlite/src/ex/db.h \
	sqlite/src/ex/dbbebdb1.c \
	sqlite/src/ex/dbbemird.c \
	sqlite/src/ex/pg.c \
	sqlite/src/ex/pg.h \
	sqlite/src/ex/sizes.tcl \
	sqlite/test/all.test \
	sqlite/test/copy.test \
	sqlite/test/dbbe.test \
	sqlite/test/delete.test \
	sqlite/test/expr.test \
	sqlite/test/func.test \
	sqlite/test/in.test \
	sqlite/test/index.test \
	sqlite/test/insert.test \
	sqlite/test/insert2.test \
	sqlite/test/lock.test \
	sqlite/test/main.test \
	sqlite/test/rowid.test \
	sqlite/test/select1.test \
	sqlite/test/select2.test \
	sqlite/test/select3.test \
	sqlite/test/select4.test \
	sqlite/test/select5.test \
	sqlite/test/sort.test \
	sqlite/test/subselect.test \
	sqlite/test/table.test \
	sqlite/test/tclsqlite.test \
	sqlite/test/tester.tcl \
	sqlite/test/trans.test \
	sqlite/test/update.test \
	sqlite/test/vacuum.test \
	sqlite/test/where.test \
	sqlite/tool/gdbmdump.c \
	sqlite/tool/gdbmstat.c \
	sqlite/tool/lemon.c \
	sqlite/tool/lempar.c \
	sqlite/tool/memleak.awk \
	sqlite/tool/opNames.awk \
	sqlite/tool/opcodeDoc.awk \
	sqlite/tool/renumberOps.awk \
	sqlite/tool/report1.txt
