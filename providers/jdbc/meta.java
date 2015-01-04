import java.sql.*;
import java.util.*;

/*
 * This class is the central point for meta data extraction. It is a default implementation for all JDBC providers
 * and can be sub-classed for database specific adaptations
 */
class GdaJMeta {
	private static native void initIDs();
	public Connection cnc;
	public DatabaseMetaData md;
	private HashMap<String, Boolean> current_schemas = new HashMap<String, Boolean> ();

	public GdaJMeta (Connection cnc) throws Exception {
		this.cnc = cnc;
		this.md = cnc.getMetaData ();
	}

	public void schemaAddCurrent (String schema) {
		current_schemas.put (GdaJValue.toLower (schema), true);
	}

	public boolean schemaIsCurrent (String schema) throws Exception {
		return current_schemas.containsKey (GdaJValue.toLower (schema));
	}

	// The returned String must _never_ be NULL, but set to "" if it was NULL
	public String getCatalog () throws Exception {
		String s = cnc.getCatalog ();
		if (s == null)
			return "";
		else
			return s;
	}

	public GdaJResultSet getSchemas (String catalog, String schema) throws Exception {
		return new GdaJMetaSchemas (this, catalog, schema);
	}

	public GdaJResultSet getTables (String catalog, String schema, String name) throws Exception {
		return new GdaJMetaTables (this, catalog, schema, name);
	}

	public GdaJResultSet getViews (String catalog, String schema, String name) throws Exception {
		return new GdaJMetaViews (this, catalog, schema, name);
	}

	public GdaJResultSet getColumns (String catalog, String schema, String tab_name) throws Exception {
		return new GdaJMetaColumns (this, catalog, schema, tab_name);
	}

	public GdaJResultSet getTableConstraints (String catalog, String schema, String table, String constraint_name) throws Exception {
		return new GdaJMetaTableConstraints (this, catalog, schema, table, constraint_name);
	}

	// class initializer
	static {
		initIDs ();
	}
}

/*
 * Meta data retrieval is returned as instances of the class which allows a degree of
 * customization depending on the type of database being accessed.
 */
abstract class GdaJMetaResultSet extends GdaJResultSet {
	public Vector<GdaJColumnInfos> meta_col_infos;
	protected ResultSet rs;
	protected DatabaseMetaData md;
	protected GdaJMeta jm;

	protected GdaJMetaResultSet (int ncols, GdaJMeta jm) {
		super (ncols);
		this.jm = jm;
		md = jm.md;
		rs = null;
		meta_col_infos = new Vector<GdaJColumnInfos> (0);
	}

	// get result set's meta data
	public GdaJResultSetInfos getInfos () throws Exception {
		return new GdaJMetaInfos (this);
	}

	abstract public boolean fillNextRow (long c_pointer) throws Exception;
}

/*
 * Extends GdaJResultSetInfos for GdaJMetaResultSet
 */
class GdaJMetaInfos extends GdaJResultSetInfos {
	private GdaJMetaResultSet meta;

	// Constructor
	public GdaJMetaInfos (GdaJMetaResultSet meta) {
		super (meta.meta_col_infos);
		this.meta = meta;
	}

	// describe a column, index starting at 0
        public GdaJColumnInfos describeColumn (int col) throws Exception {
                return meta.meta_col_infos.elementAt (col);
        }
}



/*
 * Meta data for schemas
 */
class GdaJMetaSchemas extends GdaJMetaResultSet {
	String catalog = null;
	String schema = null;

	public GdaJMetaSchemas (GdaJMeta jm, String catalog, String schema) throws Exception {
		super (4, jm);
		meta_col_infos.add (new GdaJColumnInfos ("catalog_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("schema_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("schema_owner", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("schema_internal", null, java.sql.Types.BOOLEAN));
		rs = jm.md.getSchemas ();
		this.catalog = catalog;
		this.schema = schema;
	}

	protected void columnTypesDeclared () {
		// the catalog part cannot be NULL, but "" instead
		GdaJValue cv = col_values.elementAt (0);
		cv.no_null = true;
		cv.convert_lc = true;
		(col_values.elementAt (1)).convert_lc = true;
	}

	public boolean fillNextRow (long c_pointer) throws Exception {
		if (! rs.next ())
			return false;

		GdaJValue cv;
		
		if (catalog != null) {
			String s = rs.getString (2);
			if (s != catalog)
				return fillNextRow (c_pointer);
		}
		if (schema != null) {
			String s = rs.getString (1);
			if (s != schema)
				return fillNextRow (c_pointer);
		}

		cv = col_values.elementAt (0);
		cv.setCValue (rs, 1, c_pointer);
		cv = col_values.elementAt (1);
		cv.setCValue (rs, 0, c_pointer);
		cv = col_values.elementAt (3);
		cv.setCBoolean (c_pointer, 3, false);

		return true;
	}
}

/*
 * Meta data for tables
 */
class GdaJMetaTables extends GdaJMetaResultSet {
	protected GdaJMetaTables (GdaJMeta jm) {
		super (9, jm);
		meta_col_infos.add (new GdaJColumnInfos ("table_catalog", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_schema", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_type", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("is_insertable_into", null, java.sql.Types.BOOLEAN));
		meta_col_infos.add (new GdaJColumnInfos ("table_comments", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_short_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_full_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_owner", null, java.sql.Types.VARCHAR));
		md = jm.md;
	}

	public GdaJMetaTables (GdaJMeta jm, String catalog, String schema, String name) throws Exception {
		this (jm);
		rs = md.getTables (catalog, schema, name, null);
	}

	protected void columnTypesDeclared () {
		// the catalog part cannot be NULL, but "" instead
		GdaJValue cv = col_values.elementAt (0);
		cv.no_null = true;
		cv.convert_lc = true;
		(col_values.elementAt (1)).convert_lc = true;
		(col_values.elementAt (2)).convert_lc = true;
		(col_values.elementAt (6)).convert_lc = true;
		(col_values.elementAt (7)).convert_lc = true;
	}

	public boolean fillNextRow (long c_pointer) throws Exception {
		if (! rs.next ())
			return false;

		GdaJValue cv;
		
		cv = col_values.elementAt (0);
		cv.setCValue (rs, 0, c_pointer);
		cv = col_values.elementAt (1);
		cv.setCValue (rs, 1, c_pointer);
		cv = col_values.elementAt (2);
		cv.setCValue (rs, 2, c_pointer);
		cv = col_values.elementAt (3);
		cv.setCValue (rs, 3, c_pointer);
		
		cv = col_values.elementAt (5);
		cv.setCValue (rs, 4, c_pointer);

		String ln = GdaJValue.toLower (rs.getString (2) + "." + rs.getString (3));
		if (jm.schemaIsCurrent (rs.getString (2)))
			cv.setCString (c_pointer, 6, GdaJValue.toLower (rs.getString (3)));
		else
			cv.setCString (c_pointer, 6, ln);
		cv = col_values.elementAt (7);
		cv.setCString (c_pointer, 7, ln);

		return true;
	}
}

/*
 * Meta data for views
 */
class GdaJMetaViews extends GdaJMetaResultSet {
	protected GdaJMetaViews (GdaJMeta jm) {
		super (6, jm);
		meta_col_infos.add (new GdaJColumnInfos ("table_catalog", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_schema", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("view_definition", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("check_option", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("is_updatable", null, java.sql.Types.BOOLEAN));
		md = jm.md;
	}

	public GdaJMetaViews (GdaJMeta jm, String catalog, String schema, String name) throws Exception {
		this (jm);
		rs = jm.md.getTables (catalog, schema, name, new String[]{"VIEW"});
	}

	protected void columnTypesDeclared () {
		// the catalog part cannot be NULL, but "" instead
		GdaJValue cv = col_values.elementAt (0);
		cv.no_null = true;
		cv.convert_lc = true;
		(col_values.elementAt (1)).convert_lc = true;
		(col_values.elementAt (2)).convert_lc = true;
	}

	public boolean fillNextRow (long c_pointer) throws Exception {
		if (! rs.next ())
			return false;

		GdaJValue cv;
		
		cv = col_values.elementAt (0);
		cv.setCValue (rs, 0, c_pointer);
		cv = col_values.elementAt (1);
		cv.setCValue (rs, 1, c_pointer);
		cv = col_values.elementAt (2);
		cv.setCValue (rs, 2, c_pointer);

		return true;
	}
}

/*
 * Meta data for table's columns
 */
class GdaJMetaColumns extends GdaJMetaResultSet {
	protected GdaJMetaColumns (GdaJMeta jm) {
		super (24, jm);
		meta_col_infos.add (new GdaJColumnInfos ("table_catalog", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_schema", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("column_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("ordinal_position", null, java.sql.Types.INTEGER));
		meta_col_infos.add (new GdaJColumnInfos ("column_default", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("is_nullable", null, java.sql.Types.BOOLEAN));
		meta_col_infos.add (new GdaJColumnInfos ("data_type", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("array_spec", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("gtype", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("character_maximum_length", null, java.sql.Types.INTEGER));
		meta_col_infos.add (new GdaJColumnInfos ("character_octet_length", null, java.sql.Types.INTEGER));
		meta_col_infos.add (new GdaJColumnInfos ("numeric_precision", null, java.sql.Types.INTEGER));
		meta_col_infos.add (new GdaJColumnInfos ("numeric_scale", null, java.sql.Types.INTEGER));
 		meta_col_infos.add (new GdaJColumnInfos ("datetime_precision", null, java.sql.Types.INTEGER));
		meta_col_infos.add (new GdaJColumnInfos ("character_set_catalog", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("character_set_schema", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("character_set_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("collation_catalog", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("collation_schema", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("collation_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("extra", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("is_updatable", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("column_comments", null, java.sql.Types.VARCHAR));

		md = jm.md;
	}

	public GdaJMetaColumns (GdaJMeta jm, String catalog, String schema, String tab_name) throws Exception {
		this (jm);
		rs = jm.md.getColumns (catalog, schema, tab_name, null);
	}

	protected void columnTypesDeclared () {
		// the catalog part cannot be NULL, but "" instead
		GdaJValue cv = col_values.elementAt (0);
		cv.no_null = true;
		cv.convert_lc = true;
		(col_values.elementAt (1)).convert_lc = true;
		(col_values.elementAt (2)).convert_lc = true;
		(col_values.elementAt (3)).convert_lc = true;
		(col_values.elementAt (7)).convert_lc = true;
		(col_values.elementAt (15)).convert_lc = true;
		(col_values.elementAt (16)).convert_lc = true;
		(col_values.elementAt (17)).convert_lc = true;
		(col_values.elementAt (18)).convert_lc = true;
		(col_values.elementAt (19)).convert_lc = true;
		(col_values.elementAt (20)).convert_lc = true;
	}

	public boolean fillNextRow (long c_pointer) throws Exception {
		if (! rs.next ())
			return false;

		GdaJValue cv;
		int i;

		(col_values.elementAt (0)).setCValue (rs, 0, c_pointer);
		(col_values.elementAt (1)).setCValue (rs, 1, c_pointer);
		(col_values.elementAt (2)).setCValue (rs, 2, c_pointer);
		(col_values.elementAt (3)).setCValue (rs, 3, c_pointer);
		(col_values.elementAt (4)).setCValue (rs, 16, c_pointer);
		(col_values.elementAt (5)).setCValue (rs, 12, c_pointer);
		cv = col_values.elementAt (6);
		i = rs.getInt (10);
		if (i == DatabaseMetaData.columnNoNulls)
			cv.setCBoolean (c_pointer, 6, false);
		else
			cv.setCBoolean (c_pointer, 6, true);
		(col_values.elementAt (7)).setCValue (rs, 5, c_pointer);

		(col_values.elementAt (9)).setCString (c_pointer, 9, 
								   GdaJValue.jdbc_type_to_g_type (rs.getInt (5))); // gtype

		(col_values.elementAt (11)).setCValue (rs, 15, c_pointer);
		(col_values.elementAt (12)).setCValue (rs, 8, c_pointer); // numeric_precision

		(col_values.elementAt (23)).setCValue (rs, 11, c_pointer); // comments

		return true;
	}
}

/*
 * Meta data for tables
 */
class GdaJMetaTableConstraints extends GdaJMetaResultSet {
	protected GdaJMetaTableConstraints (GdaJMeta jm) {
		super (10, jm);
		meta_col_infos.add (new GdaJColumnInfos ("constraint_catalog", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("constraint_schema", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("constraint_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_catalog", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_schema", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("table_name", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("constraint_type", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("check_clause", null, java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("is_deferrable", null, java.sql.Types.BOOLEAN));
		meta_col_infos.add (new GdaJColumnInfos ("initially_deferred", null, java.sql.Types.BOOLEAN));
		md = jm.md;
	}

	public GdaJMetaTableConstraints (GdaJMeta jm, String catalog, String schema, String table, String name) throws Exception {
		this (jm);
		/* USE:
		 * md.getPrimaryKeys()
		 * md.getExportedKeys()
		 * md.getIndexInfo(unique => true)
		 * check constraints: not supported.
		 * see http://sahits.ch/blog/?p=850
		 */
		rs = md.getTables (catalog, schema, name, null);
	}

	protected void columnTypesDeclared () {
		// the catalog part cannot be NULL, but "" instead
		GdaJValue cv = col_values.elementAt (0);
		cv.no_null = true;
		cv.convert_lc = true;
		(col_values.elementAt (1)).convert_lc = true;
		(col_values.elementAt (2)).convert_lc = true;
		(col_values.elementAt (6)).convert_lc = true;
		(col_values.elementAt (7)).convert_lc = true;
	}

	public boolean fillNextRow (long c_pointer) throws Exception {
		if (! rs.next ())
			return false;

		GdaJValue cv;

		cv = col_values.elementAt (0);
		cv.setCValue (rs, 0, c_pointer);
		cv = col_values.elementAt (1);
		cv.setCValue (rs, 1, c_pointer);
		cv = col_values.elementAt (2);
		cv.setCValue (rs, 2, c_pointer);
		cv = col_values.elementAt (3);
		cv.setCValue (rs, 3, c_pointer);

		cv = col_values.elementAt (5);
		cv.setCValue (rs, 4, c_pointer);

		String ln = GdaJValue.toLower (rs.getString (2) + "." + rs.getString (3));
		if (jm.schemaIsCurrent (rs.getString (2)))
			cv.setCString (c_pointer, 6, GdaJValue.toLower (rs.getString (3)));
		else
			cv.setCString (c_pointer, 6, ln);
		cv = col_values.elementAt (7);
		cv.setCString (c_pointer, 7, ln);

		return true;
	}
}
