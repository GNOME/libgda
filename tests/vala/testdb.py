#!python
from gi.repository import Gda

class Test:
	def __init__ (self, db):
		self.cnn = Gda.Connection.open_from_string ("SQLite", "DB_NAME=" + db, None, Gda.ConnectionOptions.NONE)
		self.cnn.update_meta_store (None)
		self.meta = self.cnn.get_meta_store ()
	
	def show_meta_table (self, table):
		m = self.meta.extract ("SELECT * FROM " + table, None)
		print m.dump_as_string ()
	
	def show_table (self, table):
		m = self.cnn.execute_select_command ("SELECT * FROM " + table)
		print m.dump_as_string ()
	
	def update (self):
		self.cnn.update_meta_store ()
	
	def execute_select (self, sql):
		return self.cnn.execute_select_command (sql)
	
	def execute_sql (self, sql):
		self.cnn.execute_non_select_command (sql)


