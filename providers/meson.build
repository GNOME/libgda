raw_spec=find_program('raw_spec.py')

subdir('reuseable')
subdir('sqlite')
if get_option('experimental')
subdir('skel-implementation')
endif

if sqlcipher_dep.found() and get_option('sqlcipher')
subdir('sqlcipher')
endif

if postgres_dep.found() and get_option('postgres')
subdir('postgres')
endif

if enable_ldap
subdir('ldap')
endif

if found_jdbc and get_option('experimental')
subdir('jdbc')
endif

if lbdb_found and get_option('experimental')
subdir('bdb')
endif

if mysql_dep.found() and get_option('mysql')
subdir('mysql')
endif

if soup_dep.found() and get_option('libsoup') and mysql_dep.found() and postgres_dep.found() and get_option('web') and get_option('experimental')
subdir('web')
endif
