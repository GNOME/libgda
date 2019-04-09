inc_postgresh = include_directories ('.')
inc_postgresh_dep = declare_dependency (include_directories : inc_postgresh)


pg_keywordshash_82 = custom_target('keywords_hash_82',
		command: [
			mkkeywordhash,
			'@INPUT@', 'V82'
			],
		capture: true,
		input: 'keywords_82.list',
		output: 'keywords_hash-82.code')

pg_keywordshash_82_dep = declare_dependency (sources: pg_keywordshash_82)

pg_keywordshash_83 = custom_target('keywords_hash_83',
		command: [
			mkkeywordhash,
			'@INPUT@', 'V83'
			],
		capture: true,
		input: 'keywords_83.list',
		output: 'keywords_hash-83.code')

pg_keywordshash_83_dep = declare_dependency (sources: pg_keywordshash_83)

pg_keywordshash_84 = custom_target('keywords_hash_84',
		command: [
			mkkeywordhash,
			'@INPUT@', 'V84'
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

pg_lemon_parserh_dep = declare_dependency(sources: pg_lemon_parserh)

postgres_gendef = executable('pg_gen_def', 'gen_def.c',
	c_args: [
		'-DIMPOSED_HEADER="'+join_paths (gda_top_build, 'libgda', 'sql-parser','token_types.h')+'"',
		'-DPARSER_HEADER_PATH="'+join_paths(meson.current_build_dir(),'parser.h')+'"'
		],
	dependencies: [pg_lemon_parserh_dep],
	install: false
	)

pg_token_types = custom_target('pg_token_types',
	command: [postgres_gendef],
	capture: true,
	input: [pg_lemon_parserh],
	output: 'postgres_token_types.h',
	install: false,
	)

pg_token_types_dep = declare_dependency(sources: pg_token_types)

libgda_pg_sources = files([
	'gda-postgres-reuseable.c',
	'gda-postgres-reuseable.h',
	'gda-postgres-parser.c',
	'gda-postgres-parser.h',
	'gda-postgres-meta.c',
	'gda-postgres-meta.h',
	])

libgda_pg_sources += pg_lemon_parserc
libgda_pg_sources += pg_lemon_parserh
libgda_pg_sources += pg_token_types

pg_reusable_dep = []
pg_reusable_dep += pg_keywordshash_82_dep
pg_reusable_dep += pg_keywordshash_83_dep
pg_reusable_dep += pg_keywordshash_84_dep
pg_reusable_dep += pg_token_types_dep
