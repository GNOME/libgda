raw_spec=find_program('raw_spec.py')

subdir('reuseable')
subdir('sqlite')
subdir('skel-implementation')

if libcrypto_dep.found ()
subdir('sqlcipher')
endif

if postgres_dep.found()
subdir('postgres')
endif

if ldap_found
subdir('ldap')
endif

if found_jdbc
subdir('jdbc')
endif

if lbdb_found
subdir('bdb')
endif

if mysql_dep.found()
subdir('mysql')
endif

if soup_dep.found() and get_option('with-libsoup')
subdir('web')
endif