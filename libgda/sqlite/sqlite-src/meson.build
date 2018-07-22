sqlite_srcc = files([
	'sqlite3.c',
	])

sqlite_srch = files([
	'sqlite3.h'
	])

sqlite_src = [
	sqlite_srcc,
	sqlite_srch
	]

sqlite_src_c_args = [
	'-DSQLITE_ENABLE_COLUMN_METADATA',
	'-DSQLITE_THREADSAFE=1',
	'-DSQLITE_API=',
	'-DSQLITE_PRIVATE='
	]

sqlite_src_deps = ldl