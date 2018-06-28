

marshallersh = custom_target('marshallersh',
	command: [glib_genmarshal,
		'--header', '--prefix=_gdaui_marshal',
		'@INPUT@'
	],
	input: ['gdaui-marshal.list'],
	output: 'gdaui-marshal.h',
	capture: true
	)

marshallersc = custom_target('marshallersc',
	command: [glib_genmarshal,
		'--body', '--prefix=_gdaui_marshal',
		'@INPUT@'
	],
	input: ['gdaui-marshal.list'],
	output: 'gdaui-marshal.c',
	capture: true
	)


marshallers_sources = files([
	'gdaui-custom-marshal.c',
	'gdaui-custom-marshal.h'
	])
marshallers_sources += marshallersh
marshallers_sources += marshallersc