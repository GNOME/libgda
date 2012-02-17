/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdadata
 * Copyright (C) Daniel Espinosa Ortiz 2011 <esodan@gmail.com>
 * 
 * libgda is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * libgda is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

using Gee;
using Gda;

namespace GdaData
{
	public class Table : Object, DbObject, DbNamedObject, DbTable
	{
		protected DbTable.TableType           _type;
		protected DbRecordCollection          _records;
		protected HashMap<string,DbFieldInfo> _fields = new HashMap<string,DbFieldInfo> ();
		protected HashMap<string,DbTable>     _depends = new HashMap<string,DbTable> ();
		protected HashMap<string,DbTable>     _referenced = new HashMap<string,DbTable> ();
		
		public Table.with_fields_info (HashMap<string,DbFieldInfo> fields)
		{
			foreach (DbFieldInfo f in fields.values) {
				_fields.set (f.name, f);
			}
		}
		// DbObject Interface
		public Connection connection { get; set; }
		public void update () throws Error 
		{
			var store = connection.get_meta_store ();
			_fields.clear ();
			string cond = "";
			
			var ctx = new Gda.MetaContext ();
			ctx.set_table ("_columns");
			ctx.set_column ("table_name", name, connection);
			// May be is necesary to set schema and catalog
			connection.update_meta_store (ctx);
			
			var vals = new HashTable<string,Value?> (str_hash,str_equal);
			vals.set ("name", name);
			if (schema == null)
			{
				var cxc = new Gda.MetaContext ();
				cxc.set_table ("_information_schema_catalog_name");
				connection.update_meta_store (cxc);
				var catm = store.extract ("SELECT * FROM _information_schema_catalog_name", null);
				catalog = new Catalog ();
				catalog.connection = connection;
				catalog.name = (string) catm.get_value_at (catm.get_column_index ("catalog_name"), 0);
				var cxs = new Gda.MetaContext ();
				cxs.set_table ("_schemata");
				connection.update_meta_store (cxs);
				var schm = store.extract ("SELECT * FROM _schemata WHERE schema_default = TRUE", null);
				schema = new Schema ();
				schema.connection = connection;
				schema.name = (string) schm.get_value_at (schm.get_column_index ("schema_name"), 0);
			}
			else
			{
				vals.set ("schema", schema.name);
				cond += " AND table_schema = ##schema::string";
			}
			if (catalog != null)
			{
				// Not using default catalog
				vals.set ("catalog", schema.catalog.name);
				cond += " AND table_catalog = ##catalog::string";
			}
			
			var mt = store.extract ("SELECT * FROM _columns WHERE table_name  = ##name::string" + cond, vals);
			for (int r = 0; r < mt.get_n_rows (); r++) 
			{
				var fi = new FieldInfo ();
				fi.name = (string) mt.get_value_at (mt.get_column_index ("column_name"), r);
				fi.desc = (string) mt.get_value_at (mt.get_column_index ("column_comments"), r);
				// Set attributes
				fi.attributes = DbFieldInfo.Attribute.NONE;
				bool fcbn = (bool) mt.get_value_at (mt.get_column_index ("is_nullable"), r);
				if (fcbn)
					fi.attributes = fi.attributes | DbFieldInfo.Attribute.CAN_BE_NULL;
				string fai = (string) mt.get_value_at (mt.get_column_index ("extra"), r);
				if (fai == "AUTO_INCREMENT")
					fi.attributes = fi.attributes | DbFieldInfo.Attribute.AUTO_INCREMENT;
				
				// Default Value
				string fdv = (string) mt.get_value_at (mt.get_column_index ("column_default"), r);
				Type ft = Gda.g_type_from_string ((string) mt.get_value_at (mt.get_column_index ("gtype"), r));
				if (fdv != null) {
					fi.attributes = fi.attributes | DbFieldInfo.Attribute.HAVE_DEFAULT;
					fi.default_value = DbField.value_from_string (fdv, ft);
				}
				
				if (ft == typeof (Gda.Numeric)) {
					int fp = (int) mt.get_value_at (mt.get_column_index ("numeric_precision"), r);
					int fs = (int) mt.get_value_at (mt.get_column_index ("numeric_scale"), r);
					fi.precision = fp;
					fi.scale = fs;
				}
				if (ft == typeof (Date)) {
					int fp = (int) mt.get_value_at (mt.get_column_index ("numeric_precision"), r);
					fi.precision = fp;
				}
				_fields.set (fi.name, fi);
			}
			// Constraints
			stdout.printf ("Cheking FiledInfo Constraints ...\n");
			ctx.set_table ("_table_constraints");
			connection.update_meta_store (ctx);
			ctx.set_table ("_key_column_usage");
			connection.update_meta_store (ctx);
			var mc = store.extract ("SELECT * FROM _table_constraints WHERE table_name  = ##name::string" + cond,
									vals);
			stdout.printf ("table REF_CONST: \n"+mc.dump_as_string ());
			for (int r = 0; r < mc.get_n_rows (); r++) 
			{
				string ct = (string) mc.get_value_at (mc.get_column_index ("constraint_type"), r);
				string cn = (string) mc.get_value_at (mc.get_column_index ("constraint_name"), r);
				vals.set ("constraint_name", cn);
				var mpk = store.extract ("SELECT * FROM _key_column_usage WHERE table_name  = ##name::string AND constraint_name = ##constraint_name::string" + cond, vals);
				var colname = (string) mpk.get_value_at (mpk.get_column_index ("column_name"), 0);
				var f = _fields.get (colname);
				f.attributes = f.attributes | DbFieldInfo.attribute_from_string (ct);
				stdout.printf("Conts type: " + ct + " : Getted Field: " + f.name + ": Attrib= " + ((int)f.attributes).to_string () + "\n");
				
				if (DbFieldInfo.Attribute.FOREIGN_KEY in f.attributes) 
				{
					ctx.set_table ("_referential_constraints");
					connection.update_meta_store (ctx);
					var mfk = store.extract ("SELECT * FROM _referential_constraints WHERE table_name  = ##name::string AND constraint_name = ##constraint_name::string" + cond, vals);
					stdout.printf ("REF_CONST: \n"+mfk.dump_as_string ());
					f.fkey = new DbFieldInfo.ForeignKey ();
					f.fkey.name = cn;
					f.fkey.refname = (string) mfk.get_value_at (mfk.get_column_index ("ref_constraint_name"), 0);
					f.fkey.reftable = new Table ();
					f.fkey.reftable.connection = connection;
					f.fkey.reftable.name = (string) mfk.get_value_at (
														mfk.get_column_index ("ref_table_name"), 0);
					f.fkey.reftable.catalog = new Catalog ();
					f.fkey.reftable.catalog.connection = connection;
					f.fkey.reftable.catalog.name = (string) mfk.get_value_at (
														mfk.get_column_index ("ref_table_catalog"), 0);
					f.fkey.reftable.schema = new Schema ();
					f.fkey.reftable.schema.connection = connection;
					f.fkey.reftable.schema.name = (string) mfk.get_value_at (
														mfk.get_column_index ("ref_table_schema"), 0);
					_depends.set (f.fkey.reftable.name, f.fkey.reftable);
					var match = (string) mfk.get_value_at (
														mfk.get_column_index ("match_option"), 0);
					f.fkey.match = DbFieldInfo.ForeignKey.match_from_string (match);
					
					var update_rule = (string) mfk.get_value_at (
														mfk.get_column_index ("update_rule"), 0);
					f.fkey.update_rule = DbFieldInfo.ForeignKey.rule_from_string (update_rule);
					var delete_rule = (string) mfk.get_value_at (
														mfk.get_column_index ("delete_rule"), 0);
					f.fkey.delete_rule = DbFieldInfo.ForeignKey.rule_from_string (delete_rule);
				}
				if (DbFieldInfo.Attribute.CHECK in  f.attributes) 
				{
					// FIXME: Implement save CHECK expression 
				}
				_fields.set (f.name, f);
			}
		}
		public void save () throws Error {}
		public void append () throws Error {}
		// DbNamedObject Interface
		public string name { get; set; }
		
		// DbTable Interface
		public DbTable.TableType table_type { get { return _type; } set { _type = value; } }
		public Collection<DbFieldInfo> fields { 
			owned get { return _fields.values; } 
		}
		public Collection<DbFieldInfo> primary_keys { 
			owned get { 
				var pk = new Gee.HashMap<string,DbFieldInfo> ();
				foreach (DbFieldInfo f in fields)
				{
					if (DbFieldInfo.Attribute.PRIMARY_KEY in f.attributes
						|| DbFieldInfo.Attribute.UNIQUE in f.attributes)
					{
						pk.set (f.name, f);
					}
				}
				return  pk.values;
			} 
		}
		public DbCatalog catalog { get; set; }
		public DbSchema  schema { get; set; }
		public Collection<DbRecord> records { 
			owned get  {
				try {
					var q = new Gda.SqlBuilder (SqlStatementType.SELECT);
					q.set_table (name);
					q.select_add_field ("*", null, null);
					var s = q.get_statement ();
					var m = this.connection.statement_execute_select (s, null);
					_records = new RecordCollection (m, this);
				}
				catch (Error e) { GLib.warning (e.message); }
				return _records;
			}
		}
		public Collection<DbTable> depends    { owned get { return _depends.values; } }
		public Collection<DbTable> referenced { 
			owned get {
				try {
					if (schema == null)
						this.update ();
					foreach (DbTable t in schema.tables) {
						t.update ();
						foreach (DbTable td in t.depends) {
							if (name == td.name && 
								schema.name == td.schema.name && 
								catalog.name == td.catalog.name)
							{
								_referenced.set (t.name, t);
								break;
							}
						}
					}
				} 
				catch (Error e) { GLib.warning (e.message); }
				return _referenced.values; 
			} 
		}
	}
}
