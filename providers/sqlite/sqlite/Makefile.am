noinst_PROGRAMS = lemon
noinst_LIBRARIES = libsqlite.a

INCLUDES = \
	-DOS_UNIX=1

lemon_SOURCES = lemon.c

libsqlite_a_SOURCES = \
	btree.c \
	btree.h \
	build.c \
	delete.c \
	expr.c \
	hash.c \
	hash.h \
	insert.c \
	main.c \
	os.c \
	os.h \
	pager.c \
	pager.h \
	parse.c \
	parse.h \
	printf.c \
	random.c \
	select.c \
	sqlite.h \
	sqliteInt.h \
	table.c \
	tokenize.c \
	update.c \
	util.c \
	vdbe.c \
	vdbe.h \
	where.c

stamp-parser: $(srcdir)/parse.y $(srcdir)/lemon
	./lemon $(srcdir)/parse.y
	touch $@

parse.c: stamp-parser
	test -f y.tab.c && mv -f y.tab.c parse.c || touch $@

parse.h: stamp-parser
	test -f y.tab.h && mv -f y.tab.h parse.h || touch $@

BUILT_SOURCES= \
	parse.h \
	parse.c \
	stamp-parser

EXTRA_DIST = \
	parse.y \
	lempar.c
