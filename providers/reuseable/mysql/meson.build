inc_mysqlh = include_directories ('.')
inc_mysqlh_dep = declare_dependency (include_directories : inc_mysqlh)

mysql_keywordshash_50 = custom_target('keywords_hash_mysql50',
		command: [
			mkkeywordhash,
			'@INPUT@', 'V50'
			],
		capture: true,
		input: 'keywords_V50.list',
		output: 'keywords_hash_V50.code')

mysql_keywordshash_50_dep = declare_dependency (sources: mysql_keywordshash_50)

mysql_keywordshash_51 = custom_target('keywords_hash_mysql51',
		command: [
			mkkeywordhash,
			'@INPUT@', 'V51'
			],
		capture: true,
		input: 'keywords_V51.list',
		output: 'keywords_hash_V51.code')

mysql_keywordshash_51_dep = declare_dependency (sources: mysql_keywordshash_51)

mysql_keywordshash_54 = custom_target('keywords_hash_mysql54',
		command: [
			mkkeywordhash,
			'@INPUT@', 'V54'
			],
		capture: true,
		input: 'keywords_V54.list',
		output: 'keywords_hash_V54.code')

mysql_keywordshash_54_dep = declare_dependency (sources: mysql_keywordshash_54)


mysql_keywordshash_60 = custom_target('keywords_hash_mysql60',
		command: [
			mkkeywordhash,
			'@INPUT@', 'V60'
			],
		capture: true,
		input: 'keywords_V60.list',
		output: 'keywords_hash_V60.code')

mysql_keywordshash_60_dep = declare_dependency (sources: mysql_keywordshash_60)

mysql_lemon_parserc = custom_target('mysql_lemon-parser_c',
	command : [lemon_exe, '-d', '-h',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.c'],
	install : false
	)


mysql_lemon_parserh = custom_target('my_lemon-parser_h',
	command : [lemon_exe, '-d', '-z',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.h'],
	install : false
	)

mysql_lemon_parserh_dep = declare_dependency(sources: mysql_lemon_parserh)

mysql_gendef = executable('my_gen_def', 'gen_def.c',
	c_args: [
		'-DIMPOSED_HEADER="'+join_paths (gda_top_build, 'libgda', 'sql-parser','token_types.h')+'"',
		'-DPARSER_HEADER_PATH="'+join_paths(meson.current_build_dir(),'parser.h')+'"'
		],
	dependencies: [mysql_lemon_parserh_dep],
	install: false
	)

mysql_token_types = custom_target('mysql_token_types',
	command: [mysql_gendef],
	capture: true,
	input: [mysql_lemon_parserh],
	output: 'mysql_token_types.h',
	install: false
	)
mysql_token_types_dep = declare_dependency(sources: mysql_token_types)

libgda_reusable_mysql_sources = files([
	'gda-mysql-reuseable.c',
	'gda-mysql-reuseable.h',
	'gda-mysql-parser.c',
	'gda-mysql-parser.h',
	'gda-mysql-meta.c',
	'gda-mysql-meta.h',
	])

libgda_reusable_mysql_sources += [
	mysql_lemon_parserh,
	mysql_lemon_parserc
	]

mysql_reusable_dep = []
mysql_reusable_dep += mysql_keywordshash_50_dep
mysql_reusable_dep += mysql_keywordshash_51_dep
mysql_reusable_dep += mysql_keywordshash_54_dep
mysql_reusable_dep += mysql_keywordshash_60_dep
mysql_reusable_dep += mysql_token_types_dep
