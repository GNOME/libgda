## Process this file with automake to produce Makefile.in

INCLUDES = -ggdb `glib-config --cflags` \
	 -Wall -Wimplicit -Wreturn-type -Wunused -Wswitch -Wcomment -Wuninitialized -Wparentheses -Wtraditional -Wpointer-arith -Wmissing-prototypes\
	 -g -fPIC 

parser.c: parser.y
	bison -v -d -o parser.c -p sql parser.y

lexer.c: lexer.l parser.c
	flex -olexer.c -Psql lexer.l

noinst_PROGRAMS = sql_parse_test

lib_LTLIBRARIES = libbondsql.la

libbondsql_la_SOURCES = parser.c lexer.c sql_parser.c mem.c sql_display.c \
		sql_tree.c 
libbondsqlincdir = $(include)/bond
libbondsqlinc_HEADERS = 
libbondsql_la_LDFLAGS = `glib-config --libs`

sql_parse_test_SOURCES=$(libbondsql_la_SOURCES) sql_parse_test.c
sql_parse_test_LDFLAGS = `glib-config --libs`
sql_parse_test_DEPENDENCIES =
sql_parse_test_LDADD = 

tests: sql_parse_test
