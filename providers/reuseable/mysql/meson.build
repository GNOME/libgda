mysql_keywordshash_50 = custom_target('keywords_hash_mysql50',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords_V50.list',
		output: 'keywords_hash_V50.code')

mysql_keywordshash_50_dep = declare_dependency (sources: mysql_keywordshash_50)

mysql_keywordshash_51 = custom_target('keywords_hash_mysql51',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords_V51.list',
		output: 'keywords_hash_V51.code')

mysql_keywordshash_51_dep = declare_dependency (sources: mysql_keywordshash_51)

mysql_keywordshash_54 = custom_target('keywords_hash_mysql54',
		command: [
			mkkeywordhash,
			'@INPUT@'
			],
		capture: true,
		input: 'keywords_V54.list',
		output: 'keywords_hash_V54.code')

mysql_keywordshash_54_dep = declare_dependency (sources: mysql_keywordshash_54)


mysql_keywordshash_60 = custom_target('keywords_hash_mysql60',
		command: [
			mkkeywordhash,
			'@INPUT@'
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


mysql_token_types = custom_target('mysql_token_types',
	command: [gendef_exe],
	capture: true,
	input: [mysql_lemon_parserh],
	output: 'mysql_token_types.h',
	install: false
	)
