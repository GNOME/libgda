/* -*- Mode: Vala; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * libgdadata
 * Copyright (C) Daniel Espinosa Ortiz 2012 <esodan@gmail.com>
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
		private bool _read_only = false;
		private int  _n_cols = -1;
		private bool _update_meta = false;
		private string _original_name = "";
		
		protected string                      _name;
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
		
		public bool update_meta { get { return _update_meta; } set {_update_meta = value;} }
		
		public void update () throws Error 
		{
			_fields.clear ();
			string cond = "";
			
			if (update_meta) {
				var ctx = new Gda.MetaContext ();
				ctx.set_table ("_columns");
				ctx.set_column ("table_name", name, connection);
				try { connection.update_meta_store (ctx); }
				catch (Error e) {
					GLib.message ("Updating meta store for _columns fails with error: " + e.message+"\n");
					GLib.message ("Trying to update all...");
					update_meta = !connection.update_meta_store (null);
				}
			}
			
			var store = connection.get_meta_store ();
			
			var vals = new HashTable<string,Value?> (str_hash,str_equal);
			vals.set ("name", name);
			if (schema == null)
			{
				if (update_meta) {
					try { 
						var cxc = new Gda.MetaContext ();
						cxc.set_table ("_information_schema_catalog_name");
						connection.update_meta_store (cxc); 
					}
					catch (Error e) {
						GLib.message ("Updating meta store for _information_schema_catalog_name fails with error: "
										 + e.message+"\n");
						GLib.message ("Trying to update all...");
						update_meta = !connection.update_meta_store (null);
						if (!update_meta)
							GLib.message ("Meta store update ... Ok");
					}
				}
				var catm = store.extract ("SELECT * FROM _information_schema_catalog_name", null);
				catalog = new Catalog ();
				catalog.connection = connection;
				catalog.name = (string) catm.get_value_at (catm.get_column_index ("catalog_name"), 0);
				if (update_meta) {
					try { 
						var cxs = new Gda.MetaContext ();
						cxs.set_table ("_schemata");
						connection.update_meta_store (cxs); 
					}
					catch (Error e) {
						GLib.message ("Updating meta store for _schemata fails with error: " + e.message+"\n");
						GLib.message ("Trying to update all...");
						update_meta = connection.update_meta_store (null);
						if (update_meta)
							GLib.message ("Meta store update ... Ok");
					}
				}
				var schm = store.extract ("SELECT * FROM _schemata WHERE schema_default = TRUE", null);
				schema = new Schema ();
				schema.catalog = catalog;
				schema.connection = connection;
				schema.name = (string) schm.get_value_at (schm.get_column_index ("schema_name"), 0);
			}

			vals.set ("schema", schema.name);
			cond += " AND table_schema = ##schema::string";
			
			if (catalog != null)
			{
				// Not using default catalog
				vals.set ("catalog", schema.catalog.name);
				cond += " AND table_catalog = ##catalog::string";
			}
			
			var mt = store.extract ("SELECT * FROM _columns"+
			                       " WHERE table_name  = ##name::string" + cond, vals);
			int r;
			for (r = 0; r < mt.get_n_rows (); r++) 
			{
				var fi = new FieldInfo ();
				fi.name = (string) mt.get_value_at (mt.get_column_index ("column_name"), r);
				Value v = mt.get_value_at (mt.get_column_index ("column_comments"), r);
				if (v.holds (typeof (string)))
					fi.desc = (string) v;
				else
					fi.desc = "";
				fi.ordinal = (int) mt.get_value_at (mt.get_column_index ("ordinal_position"), r);
				// Set attributes
				fi.attributes = DbFieldInfo.Attribute.NONE;
				bool fcbn = (bool) mt.get_value_at (mt.get_column_index ("is_nullable"), r);
				if (fcbn)
					fi.attributes = fi.attributes | DbFieldInfo.Attribute.CAN_BE_NULL;
				string fai;
				v = mt.get_value_at (mt.get_column_index ("extra"), r);
				if (v.holds (typeof (string)))
					fai = (string) v;
				else
					fai = "";
				if (fai == "AUTO_INCREMENT")
					fi.attributes = fi.attributes | DbFieldInfo.Attribute.AUTO_INCREMENT;
				// Default Value
				string fdv;
				v = mt.get_value_at (mt.get_column_index ("column_default"), r);
				if (v.holds (typeof (string)))
					fdv = (string) v;
				else
					fdv = "";
				Type ft = Gda.g_type_from_string ((string) mt.get_value_at (mt.get_column_index ("gtype"), r));
				fi.value_type = ft;
				if (fdv != null) {
					fi.attributes = fi.attributes | DbFieldInfo.Attribute.HAVE_DEFAULT;
					fi.default_value = DbField.value_from_string (fdv, ft);
				}
				
				if (ft == typeof (string)) {
					var hld = connection.get_provider ().get_data_handler_g_type (connection, typeof (string));
					Value strv = (string) hld.get_value_from_sql ((string) fi.default_value, typeof (string));
					fi.default_value = (string) strv;
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
			if (update_meta) {
				try { 
					var cxcr = new Gda.MetaContext ();
					cxcr.set_table ("_table_constraints");
					connection.update_meta_store (cxcr); 
				}
				catch (Error e) {
					GLib.message ("Updating meta store for _table_constraints usage fails with error: " 
									+ e.message+"\n");
					GLib.message ("Trying to update all...");
					update_meta = connection.update_meta_store (null);
					if (update_meta)
							GLib.message ("Meta store update ... Ok");
				}
			}
			if (update_meta) {
				try { 
					var cxcr2 = new Gda.MetaContext ();
					cxcr2.set_table ("_key_column_usage");
					connection.update_meta_store (cxcr2); 
				}
				catch (Error e) {
					GLib.message ("Updating meta store for _key_column_usage usage fails with error: "
									 + e.message+"\n");
					GLib.message ("Trying to update all...");
					update_meta = connection.update_meta_store (null);
					if (update_meta)
							GLib.message ("Meta store update ... Ok");
				}
			}
			var mc = store.extract ("SELECT * FROM _table_constraints"+
			                        " WHERE table_name  = ##name::string" + cond,
									vals);
			for (r = 0; r < mc.get_n_rows (); r++) 
			{
				string ct = (string) mc.get_value_at (mc.get_column_index ("constraint_type"), r);
				string cn = (string) mc.get_value_at (mc.get_column_index ("constraint_name"), r);
				vals.set ("constraint_name", cn);
				var mpk = store.extract ("SELECT * FROM _key_column_usage"+
				                         " WHERE table_name  = ##name::string"+
				                         " AND constraint_name = ##constraint_name::string" + cond, vals);
				if (mpk.get_n_rows () != 1) {
					GLib.warning ("Constraint '" + cn + "' type '" + ct + "' not found. Skipped!");
					continue;
				}
				var colname = (string) mpk.get_value_at (mpk.get_column_index ("column_name"), 0);
				var f = _fields.get (colname);
				f.attributes = f.attributes | DbFieldInfo.attribute_from_string (ct);
				
				if (DbFieldInfo.Attribute.FOREIGN_KEY in f.attributes) 
				{
					if (update_meta) {
						try { 
							var cxcr3 = new Gda.MetaContext ();
							cxcr3.set_table ("_referential_constraints");
							connection.update_meta_store (cxcr3); 
						}
						catch (Error e) {
							GLib.message ("Updating for _referential_constraints usage fails with error: " 
											+ e.message+"\n");
							GLib.message ("Trying to update all...");
							update_meta = connection.update_meta_store (null);
							if (update_meta)
									GLib.message ("Meta store update ... Ok");
						}
					}
					var mfk = store.extract ("SELECT * FROM _referential_constraints"+
					                         " WHERE table_name  = ##name::string "+
					                         "AND constraint_name = ##constraint_name::string" + cond, vals);
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
					
					var mfkr = store.extract ("SELECT * FROM _detailed_fk " + 
											" WHERE fk_table_name  = ##name::string"+
				                         " AND fk_constraint_name = ##constraint_name::string" +
				                         " AND fk_table_catalog = ##catalog::string"+
				                         " AND fk_table_schema = ##schema::string", vals);
					for (int r2 = 0; r2 < mfkr.get_n_rows (); r2++) {
						var rc = (string) mfkr.get_value_at (mfkr.get_column_index ("ref_column"), r2);
						f.fkey.refcol.add (rc);
					}
				}
				if (DbFieldInfo.Attribute.CHECK in  f.attributes) 
				{
					// FIXME: Implement save CHECK expression 
				}
				_fields.set (f.name, f);
			}
			
			// Table referencing this
			
			var mtr = store.extract ("SELECT * FROM "+
			                     	 "_detailed_fk WHERE ref_table_name  = ##name::string"+
			                         " AND ref_table_catalog = ##catalog::string"+
			                         " AND ref_table_schema = ##schema::string", vals);
			if (mtr.get_n_rows () == 0)
				GLib.message ("Rows = 0");
			for (r = 0; r < mtr.get_n_rows (); r++) {
				var tn = (string) mtr.get_value_at (mtr.get_column_index ("fk_table_name"), r);
				var tr = new Table ();
				tr.connection = connection;
				tr.name = tn;
				_referenced.set (tr.name, tr);
			}
			_read_only = true;
			_n_cols = _fields.size;
		}
		
		public void save () throws Error 
		{
			GLib.warning ("Not Implemented!");
			return;
			// FIXME: ServerOperation Bug or biding Bug
			/*if (GLib.strcmp (_name,_original_name) != 0) {
				if (connection.get_provider ()
					.supports_operation (connection, 
						Gda.ServerOperationType.RENAME_TABLE, null)) {
					var op = connection.get_provider ().create_operation (connection,
								Gda.ServerOperationType.RENAME_TABLE,
								null);
					op.set_value_at (_original_name, "/TABLE_DEF_P/TABLE_NAME");
					op.set_value_at (name, "/TABLE_DEF_P/TABLE_NEW_NAME");
					stdout.printf ("Operation to perform: "+ 
						connection.get_provider ()
							.render_operation (connection,op));
					connection.get_provider ().perform_operation (connection, op);
				}
				else {
					GLib.warning ("Provider doesn't support table rename\n");
				}
			}
			else {
				throw new DbTableError.READ_ONLY ("Table definition is read only");
			}*/
		}
		
		public void append () throws Error 
		{
			var op = connection.create_operation (Gda.ServerOperationType.CREATE_TABLE, null);
			op.set_value_at (name, "/TABLE_DEF_P/TABLE_NAME");
			if (DbTable.TableType.LOCAL_TEMPORARY in table_type)
				op.set_value_at ("TRUE","/TABLE_DEF_P/TABLE_TEMP");
			int refs = 0;
			foreach (DbFieldInfo f in fields) {
				op.set_value_at (f.name, "/FIELDS_A/@COLUMN_NAME/" + f.ordinal.to_string ());
				op.set_value_at (connection.get_provider ().get_default_dbms_type (connection, f.value_type), 
										"/FIELDS_A/@COLUMN_TYPE/" + f.ordinal.to_string ());
				if (DbFieldInfo.Attribute.PRIMARY_KEY in f.attributes) {
					op.set_value_at ("TRUE", "/FIELDS_A/@COLUMN_PKEY/" + f.ordinal.to_string ());
				}
				if (DbFieldInfo.Attribute.CAN_BE_NULL in f.attributes) {
					op.set_value_at ("TRUE", "/FIELDS_A/@COLUMN_NNUL/" + f.ordinal.to_string ());
				}
				if (DbFieldInfo.Attribute.AUTO_INCREMENT in f.attributes) {
					op.set_value_at ("TRUE", "/FIELDS_A/@COLUMN_AUTOINC/" + f.ordinal.to_string ());
				}
				if (DbFieldInfo.Attribute.UNIQUE in f.attributes) {
					op.set_value_at ("TRUE", "/FIELDS_A/@COLUMN_UNIQUE/" + f.ordinal.to_string ());
				}
				if (DbFieldInfo.Attribute.HAVE_DEFAULT in f.attributes) {
					op.set_value_at (connection.value_to_sql_string (f.default_value),
											"/FIELDS_A/@COLUMN_DEFAULT/" + f.ordinal.to_string ());
				}
				if (DbFieldInfo.Attribute.FOREIGN_KEY in f.attributes) {
					op.set_value_at (f.fkey.reftable.name, "/FKEY_S/" + refs.to_string () + 
									"/FKEY_REF_TABLE");
					int rc = 0;
					foreach (string fkc in f.fkey.refcol) {
						op.set_value_at (f.name, "/FKEY_S/" + refs.to_string ()
										+ "/FKEY_FIELDS_A/@FK_FIELD/" + rc.to_string ());
						op.set_value_at (fkc, "/FKEY_S/" + refs.to_string ()
										+ "/FKEY_FIELDS_A/@FK_REF_PK_FIELD/" + rc.to_string ());
						rc++;
					}

					op.set_value_at (DbFieldInfo.ForeignKey.rule_to_string (f.fkey.update_rule),
										"/FKEY_S/" + refs.to_string () + "/FKEY_ONUPDATE");
					op.set_value_at (DbFieldInfo.ForeignKey.rule_to_string (f.fkey.delete_rule),
										"/FKEY_S/" + refs.to_string () + "/FKEY_ONDELETE");
					refs++;
				}
			}
			connection.perform_operation (op);
		}
		
		public void drop (bool cascade) throws Error
		{
			var op = Gda.ServerOperation.prepare_drop_table (connection, name);
			op.perform_drop_table ();
		}

		// DbNamedObject Interface
		
		public string name 
		{ 
			get { return _name; }
			set 
			{
				if (GLib.strcmp ("",_original_name) == 0) {
					_original_name = value;
				}
				_name = value;
			}
		}
		
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
					q.select_add_target (name, null);
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
			owned get { return _referenced.values; } 
		}
		
		public void set_field (DbFieldInfo field) throws Error
		{
			if (_read_only) {
				throw new DbTableError.READ_ONLY ("Table's definition is read only");
			}
			if (field.ordinal < 0) {
				_n_cols++;
				field.ordinal = _n_cols;
			}
			_fields.set (field.name, field);
		}
		
		public DbFieldInfo get_field (string name) throws Error
		{
			if (!_fields.has_key (name))
				throw new DbTableError.FIELD ("Field '" + name + "' not found");
			return _fields.get (name);
		}
	}
}
