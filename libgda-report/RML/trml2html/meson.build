trml2html_install_dir = join_paths(get_option('datadir'), project_package, 'gda_trml2html')

install_data('trml2html.py',
	install_dir: trml2html_install_dir,
	install_mode: 'rwxr-xr-x',
	)
trml2html = files(['__init__.py', 'utils.py'])
install_data(trml2html,
	install_dir: trml2html_install_dir
	)
