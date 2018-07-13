docconf = configuration_data()
docconf.set('LIBGDA_LDAP_TYPE',LIBGDA_LDAP_TYPE)
docconf.set('LIBGDA_LDAP_TYPE2',LIBGDA_LDAP_TYPE2)
docconf.set('LIBGDA_LDAP_TYPE3',LIBGDA_LDAP_TYPE3)

libgda_typesdoc = configure_file(
		input: 'libgda.types.in',
		output : 'libgda.types',
		configuration : docconf)