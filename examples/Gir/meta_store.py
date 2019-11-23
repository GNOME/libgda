#!/usr/bin/env python3
from gi.repository import GLib
from gi.repository import Gda
print Gda
print "Initializing Database..."
GLib.unlink ("meta_store.db")
c = Gda.Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=meta_store", None, Gda.ConnectionOptions.NONE)
c.execute_non_select_command("CREATE TABLE user (name string PRIMARY KEY, functions string, security_number integer)")
c.execute_non_select_command("INSERT INTO user (name, functions, security_number) VALUES ( \"Martin Stewart\", \"Programmer, QA\", 2334556)")
m = c.execute_select_command ("SELECT * FROM user")
print m.dump_as_string()
print "Initializing Done..."
print "Updating Meta Store for table 'user'..."
cx = Gda.MetaContext ()
cx.set_table ("_tables")
cx.set_column ("table_name", "user", c)
c.update_meta_store (cx)
ms = c.get_meta_store ()
metatables = ms.extract ("SELECT * FROM _tables", None)
print metatables.dump_as_string ()
print "Adding a new table..."
c.execute_non_select_command("CREATE TABLE customers (name string PRIMARY KEY, description string)")
c.execute_non_select_command("INSERT INTO customers (name, description) VALUES ( \"IBMac\", \"International BMac\")")

print "Updating Meta Store for table 'customers'..."
cx.set_column ("table_name", "customers", c)
c.update_meta_store (cx)
metatables2 = ms.extract ("SELECT * FROM _tables", None)
print metatables2.dump_as_string ()

print "Dropping table 'customers' to re-create it with different columns..."
print "Showing 'customers' columns"
f = ms.extract ("SELECT * FROM _columns WHERE table_name = \"customers\"", None)
print f.dump_as_string ()
c.execute_non_select_command("DROP TABLE customers")
c.execute_non_select_command("CREATE TABLE customers (name string PRIMARY KEY, account integer, description string)")
c.execute_non_select_command("INSERT INTO customers (name, account, description) VALUES ( \"IBMac\", 30395, \"International BMac\")")

print "Updating Meta Store for table 'customers' columns..."
cx.set_table ("_columns")
c.update_meta_store (cx)
f2 = ms.extract ("SELECT * FROM _columns WHERE table_name = \"customers\"", None)
print f2.dump_as_string ()

print "Updating Meta Store for catalog and schemas..."
cx2 = Gda.MetaContext ()
cx2.set_table ("_information_schema_catalog_name")
c.update_meta_store (cx2)
cm = ms.extract ("SELECT * FROM _information_schema_catalog_name", None)
print "Catalog:\n" + cm.dump_as_string ()

