raw_spec=find_program('raw_spec.py')

subdir('reuseable')
subdir('sqlite')
subdir('skel-implementation')

if sqlcipher_dep.found() and not get_option('disable-sqlcipher')
subdir('sqlcipher')
endif

if postgres_dep.found()
subdir('postgres')
endif

if ldap_found and get_option('enable-ldap') and get_option('experimental')
subdir('ldap')
endif

if found_jdbc and get_option('experimental')
subdir('jdbc')
endif

if lbdb_found and get_option('experimental')
subdir('bdb')
endif

if mysql_dep.found()
subdir('mysql')
endif

if soup_dep.found() and get_option('with-libsoup') and mysql_dep.found() and postgres_dep.found() and get_option('enable-web') and get_option('experimental')
subdir('web')
endif
