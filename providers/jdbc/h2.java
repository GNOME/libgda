import java.sql.*;
import java.util.*;
import java.io.*;


class org_h2_DriverMeta extends GdaJMeta {
	public org_h2_DriverMeta (Connection cnc) throws Exception {
		super (cnc);
		ResultSet rs;
		rs = cnc.getMetaData().getSchemas();
		while (rs.next ()) {
			if (rs.getBoolean (3))
				schemaAddCurrent (rs.getString (1));
		}
	}

	public String getCatalog () throws Exception {
		return cnc.getCatalog ();
	}

	public GdaJResultSet getSchemas (String catalog, String schema) throws Exception {
		return new org_h2_DriverMetaSchemas (this, catalog, schema);
	}

	public GdaJResultSet getTables (String catalog, String schema, String name) throws Exception {
		return new org_h2_DriverMetaTables (this, catalog, schema, name);
	}

	public GdaJResultSet getViews (String catalog, String schema, String name) throws Exception {
		ResultSet rs;
		if ((catalog != null) && (schema != null)) {
			PreparedStatement ps;
			if (name != null) {
				ps = cnc.prepareStatement ("SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, VIEW_DEFINITION, CHECK_OPTION, IS_UPDATABLE::boolean FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_CATALOG = ? AND TABLE_SCHEMA = ? AND TABLE_NAME = ?");
				ps.setString (3, name);
			}
			else
				ps = cnc.prepareStatement ("SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, VIEW_DEFINITION, CHECK_OPTION, IS_UPDATABLE::boolean FROM INFORMATION_SCHEMA.VIEWS WHERE TABLE_CATALOG = ? AND TABLE_SCHEMA = ?");
			ps.setString (1, catalog);
			ps.setString (2, schema);
			rs = ps.executeQuery();
		}
		else {
			Statement stmt = cnc.createStatement ();
			rs = stmt.executeQuery ("SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, VIEW_DEFINITION, CHECK_OPTION, IS_UPDATABLE::boolean FROM INFORMATION_SCHEMA.VIEWS");
		}
		//return new GdaJResultSet (rs);
		return new org_h2_DriverMetaViews (rs);
	}
}

/*
 * Meta data for schemas
 */
class org_h2_DriverMetaSchemas extends GdaJMetaResultSet {
	ResultSet rs;
	String catalog = null;
	String schema = null;

	public org_h2_DriverMetaSchemas (GdaJMeta jm, String catalog, String schema) throws Exception {
		super (4, jm);
		meta_col_infos.add (new GdaJColumnInfos ("catalog_name", "catalog_name", java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("schema_name", "schema_name", java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("schema_owner", "owner", java.sql.Types.VARCHAR));
		meta_col_infos.add (new GdaJColumnInfos ("schema_internal", "is internal", java.sql.Types.BOOLEAN));
		rs = jm.md.getSchemas ();
		this.catalog = catalog;
		this.schema = schema;
	}

	protected void columnTypesDeclared () {
		GdaJValue cv = (GdaJValue) col_values.elementAt (0);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (1);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (3);
		cv.convert_lc = true;
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

		cv = (GdaJValue) col_values.elementAt (0);
		cv.setCValue (rs, 1, c_pointer);
		cv = (GdaJValue) col_values.elementAt (1);
		cv.setCValue (rs, 0, c_pointer);
		cv = (GdaJValue) col_values.elementAt (3);
		cv.setCBoolean (c_pointer, 3, false);

		return true;
	}
}

/*
 * Meta data for tables
 */
class org_h2_DriverMetaTables extends GdaJMetaTables {
	public org_h2_DriverMetaTables (GdaJMeta jm, String catalog, String schema, String name) throws Exception {
		super (jm);
		rs = md.getTables (catalog, schema, name, null);
	}

	protected void columnTypesDeclared () {
		GdaJValue cv = (GdaJValue) col_values.elementAt (0);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (1);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (2);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (3);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (5);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (6);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (7);
		cv.convert_lc = true;
	}

	public boolean fillNextRow (long c_pointer) throws Exception {
		if (! rs.next ())
			return false;

		GdaJValue cv;
		
		cv = (GdaJValue) col_values.elementAt (0);
		cv.setCValue (rs, 0, c_pointer);
		cv = (GdaJValue) col_values.elementAt (1);
		cv.setCValue (rs, 1, c_pointer);
		cv = (GdaJValue) col_values.elementAt (2);
		cv.setCValue (rs, 2, c_pointer);
		cv = (GdaJValue) col_values.elementAt (3);
		cv.setCValue (rs, 3, c_pointer);
		
		cv = (GdaJValue) col_values.elementAt (5);
		cv.setCValue (rs, 4, c_pointer);

		cv = (GdaJValue) col_values.elementAt (6);
		String ln = GdaJValue.toLower (rs.getString (2) + "." + rs.getString (3));
		if (jm.schemaIsCurrent (rs.getString (2)))
			cv.setCString (c_pointer, 6, GdaJValue.toLower (rs.getString (3)));
		else
			cv.setCString (c_pointer, 6, ln);
		cv = (GdaJValue) col_values.elementAt (7);
		cv.setCString (c_pointer, 7, ln);

		return true;
	}
}

/*
 * Meta data for views
 */
class org_h2_DriverMetaViews extends GdaJResultSet {
	public org_h2_DriverMetaViews (ResultSet rs) throws Exception {
		super (rs);
	}

	protected void columnTypesDeclared () {
		GdaJValue cv = (GdaJValue) col_values.elementAt (0);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (1);
		cv.convert_lc = true;
		cv = (GdaJValue) col_values.elementAt (2);
		cv.convert_lc = true;
	}
}
