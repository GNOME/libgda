gda_examples_ddl_source = ['ddl.c'] 

executable('ddl',
           sources : gda_examples_ddl_source,
           link_with: libgda,
           include_directories : example_inc_dir, 
           dependencies : [glib_dep,libxml_dep,gobject_dep],
           install: false)

