import java.sql.*;


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
 * Meta data for tables
 */
class org_h2_DriverMetaTables extends GdaJMetaTables {
	public org_h2_DriverMetaTables (GdaJMeta jm, String catalog, String schema, String name) throws Exception {
		super (jm);
		rs = md.getTables (catalog, schema, name, null);
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

		cv = col_values.elementAt (6);
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
class org_h2_DriverMetaViews extends GdaJResultSet {
	public org_h2_DriverMetaViews (ResultSet rs) throws Exception {
		super (rs);
	}

	protected void columnTypesDeclared () {
		GdaJValue cv = col_values.elementAt (0);
		cv.no_null = true;
		cv.convert_lc = true;
		(col_values.elementAt (1)).convert_lc = true;
		(col_values.elementAt (2)).convert_lc = true;
	}
}
