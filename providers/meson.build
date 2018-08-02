raw_spec=find_program('raw_spec.py')

subdir('reuseable')
subdir('sqlite')
subdir('skel-implementation')

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