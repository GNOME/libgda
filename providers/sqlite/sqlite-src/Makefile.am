noinst_LTLIBRARIES = \
	libsqlite.la
AM_CPPFLAGS = -DSQLITE_ENABLE_COLUMN_METADATA -DSQLITE_PRIVATE=

# Download the Windows prepared SQLite (series 3.x) sources in a ZIP, and
# extract the zip in this directory (thanks to http://aaronbock.net/articles/sqlite!)
#
# files list generated with: ls *.c *.h | sort | sed -e 's/$/ \\/' -e 's/^/\t/'
# and remove shell.c and tclsqlite.c
# 
# As of version 3.3.14, SQLite now ships only two files: sqlite3.c (an aggregate of all the source files) and sqlite3.h.
# Libgda needs to use the sqliteInt.h file which can be generated using
# sed -n -e '/Begin file sqliteInt.h/,/End of sqliteInt.h/p' sqlite3.c > sqliteInt.h
# and remove the "#define SQLITE_AMALGAMATION 1" line from sqlite3.c
libsqlite_la_SOURCES = \
	sqlite3.c \
	sqlite3.h 
