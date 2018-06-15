
pg_keywordshash_82 = custom_target('keywords_hash_82',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords_82.list',
		output: 'keywords_hash-82.code')

pg_keywordshash_82_dep = declare_dependency (sources: pg_keywordshash_82)

pg_keywordshash_83 = custom_target('keywords_hash_83',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords_83.list',
		output: 'keywords_hash-83.code')

pg_keywordshash_83_dep = declare_dependency (sources: pg_keywordshash_83)

pg_keywordshash_84 = custom_target('keywords_hash_84',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords_84.list',
		output: 'keywords_hash-84.code')

pg_keywordshash_84_dep = declare_dependency (sources: pg_keywordshash_84)

pg_lemon_parserc = custom_target('pg_lemon-parser_c',
	command : [lemon_exe, '-d', '-h',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.c'],
	install : false
	)


pg_lemon_parserh = custom_target('pg_lemon-parser_h',
	command : [lemon_exe, '-d', '-z',
	'-o'+meson.current_build_dir(),
	'-T'+'@INPUT0@', '@INPUT1@'],
	input : [lemon_templatef, 'parser.y'],
	output : ['parser.h'],
	install : false
	)

pg_token_types = custom_target('pg_token_types',
	command: [gendef_exe],
	capture: true,
	input: [pg_lemon_parserh],
	output: 'postgres_token_types.h',
	install: false
	)

libgda_postgres_sources = files([
	'gda-postgres-reuseable.c',
	'gda-postgres-reuseable.h',
	'gda-postgres-parser.c',
	'gda-postgres-parser.h',
	'gda-postgres-meta.c',
	'gda-postgres-meta.h',
	])