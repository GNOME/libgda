sqlite_src = files([
	'sqlite3.c',
	'sqlite3.h'
	])

sqlite_src_c_args = [
	'-DSQLITE_ENABLE_COLUMN_METADATA',
	'-DSQLITE_THREADSAFE=1',
	'-DSQLITE_API=',
	'-DSQLITE_PRIVATE='
	]

sqlite_src_deps = ldl