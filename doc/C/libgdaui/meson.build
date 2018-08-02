docui_conf = configuration_data()
docui_conf.set('GDA_VERSION', project_version)
docui_conf.set('GDA_BUILDDATE', dates.stdout())

doc_version = configure_file(
	input: 'version.xml.in',
	output: 'version.xml',
	configuration: docui_conf
	)