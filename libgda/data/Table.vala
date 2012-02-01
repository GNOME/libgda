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
		protected HashMap<string,DbTable>     _fk_depends = new HashMap<string,DbTable> ();
		protected HashMap<string,DbTable>     _fk = new HashMap<string,DbTable> ();
		
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
			var vals = new HashTable<string,Value?> (str_hash,str_equal);
			vals.set ("name", name);
			vals.set ("schema", schema.name);
			vals.set ("catalog", schema.catalog.name);
			var mt = store.extract_v ("SELECT * FROM _columns WHERE table_schema = ##schema::string AND table_catalog = ##catalog::string AND table_name  = ##name::string", vals);
			for (int r = 0; r < mt.get_n_columns (); r++) 
			{
				var fi = new FieldInfo ();
				string fname = (string) mt.get_value_at (mt.get_column_index ("column_name"), r);
				Type ft = (Type) mt.get_value_at (mt.get_column_index ("gtype"), r);
				DbFieldInfo.Attribute attr = DbFieldInfo.Attribute.NONE;
				bool fcbn = (bool) mt.get_value_at (mt.get_column_index ("is_nullable"), r);
				if (fcbn)
					attr = attr & DbField.Attribute.CAN_BE_NULL;
				string fdv = (string) mt.get_value_at (mt.get_column_index ("column_default"), r);
				if (fdv != null) {
					attr = attr & DbField.Attribute.CAN_BE_DEFAULT;
					//Value? dv = Gda.value_new_from_string (fdv, ft);
				}
				string fai = (string) mt.get_value_at (mt.get_column_index ("extras"), r);
				bool fai_b = false;
				if (fai == "AUTO_INCREMENT")
					fai_b = true;
				attr = attr & DbFieldInfo.Attribute.IS_AUTO_INCREMENT;
				if (ft == typeof (Gda.Numeric)) {
					int fp = (int) mt.get_value_at (mt.get_column_index ("numeric_precision"), r);
					int fs = (int) mt.get_value_at (mt.get_column_index ("numeric_scale"), r);
//					fi.name = fname;
//					fi.attribute = attr;
//					fi.precision = fp;
//					fi.scale = fs;
				}
				if (ft == typeof (Date)) {
					int fp = (int) mt.get_value_at (mt.get_column_index ("numeric_precision"), r);
//					fi
				}
				_fields.set (fi.name, fi);
			}
			// Constraints
			var mc = store.extract_v ("SELECT * FROM _table_constraints WHERE table_schema = ##schema::string AND table_catalog = ##catalog::string AND table_name  = ##name::string", vals);
			for (int r = 0; r < mc.get_n_columns (); r++) 
			{
				string ct = (string) mc.get_value_at (mc.get_column_index ("constraint_type"), r);
				string cn = (string) mc.get_value_at (mc.get_column_index ("constraint_name"), r);
				var vals2 = new HashTable<string,Value?> (str_hash,str_equal);
				vals2.set ("name", name);
				vals2.set ("schema", schema);
				vals2.set ("catalog", schema.catalog.name);
				vals2.set ("constraint_name", cn);
				var mpk = store.extract_v ("SELECT * FROM _key_column_usage WHERE table_schema = ##schema::string AND table_catalog = ##catalog::string AND table_name  = ##name::string AND constraint_name = ##constraint", vals2);
				var colname = (string) mpk.get_value_at (mpk.get_column_index ("column_name"), r);
				
				var f = _fields.get (colname);
				f.attributes = f.attributes & DbFieldInfo.attribute_from_string (ct);
				
				if (DbFieldInfo.Attribute.FOREIGN_KEY in f.attributes) 
				{
					f.attributes = f.attributes & DbFieldInfo.Attribute.FOREIGN_KEY;
					var mfk = store.extract_v ("SELECT * FROM _referential_constraints WHERE table_schema = ##schema::string AND table_catalog = ##catalog::string AND table_name  = ##name::string AND constraint_name = ##constraint", vals2);
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
					
				}
			}
		}
		public void save () throws Error {}
		public void append () throws Error {}
		// DbNamedObject Interface
		public string name { get; set; }
		
		// DbTable Interface
		public DbTable.TableType table_type { get { return _type; } set { _type = value; } }
		public HashMap<string,DbFieldInfo> fields { 
			get { return _fields; } 
		}
		// FIXME: Implement
//		public Iterator<DbFieldInfo> pk_fields { 
//			get { 
//				return _fields.filter (
//					(g) => {
//						if (DbFieldInfo.Attribute.PRIMARY_KEY in g.value.attributes)
//							return true;
//						return false;
//					}
//				); 
//			} 
//		}
		public DbCatalog catalog { get; set; }
		public DbSchema  schema { get; set; }
		public Collection<DbRecord> records { 
			owned get  {
				var q = new Gda.SqlBuilder (SqlStatementType.SELECT);
				q.set_table (name);
				q.select_add_field ("*", null, null);
				var s = q.get_statement ();
	    		var m = this.connection.statement_execute_select (s, null);
				_records = new RecordCollection (m, this);
				return _records;
			}
		}
		public Collection<DbTable> fk_depends { owned get { return _fk_depends.values; } }
		public Collection<DbTable> fk { owned get { return _fk.values; } }
	}
}
