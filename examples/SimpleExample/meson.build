gda_examples_example_source = ['example.c'] 

executable('simpleexample',
           sources : gda_examples_example_source,
           link_with: libgda,
           include_directories : example_inc_dir, 
           dependencies : [glib_dep,libxml_dep,gobject_dep],
           install: false)

