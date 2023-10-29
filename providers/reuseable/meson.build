if postgres_dep.found () and get_option('postgres')
subdir('postgres')
endif
if mysql_dep.found () and get_option('mysql')
subdir('mysql')
endif

libreuseable_sources = files([
	'gda-provider-reuseable.h',
	'reuse-all.c',
	'reuse-all.h'
	])