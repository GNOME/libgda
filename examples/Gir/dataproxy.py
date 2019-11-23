#!/usr/bin/env python3
from gi.repository import GLib
from gi.repository import Gda
print Gda
GLib.unlink ("dataproxy.db")
c = Gda.Connection.open_from_string("SQLite", "DB_DIR=.;DB_NAME=dataproxy", None, Gda.ConnectionOptions.NONE)
c.execute_non_select_command("CREATE TABLE user (name string PRIMARY KEY, functions string, security_number integer)")
c.execute_non_select_command("INSERT INTO user (name, functions, security_number) VALUES ( \"Martin Stewart\", \"Programmer, QA\", 2334556)")
m = c.execute_select_command ("SELECT * FROM user")
print m.dump_as_string()
print "Tesing old API for Gda.DataProxy"
pxy1 = Gda.DataProxy.new (m)
print pxy1.dump_as_string ()
print "Tesing new (5.2) API for Gda.DataProxy"
pxy2 = Gda.DataProxy.new_with_data_model (m)
print pxy2.dump_as_string ()

