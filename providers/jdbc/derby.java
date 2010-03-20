import java.sql.*;

class org_apache_derbyMeta extends GdaJMeta {
	public org_apache_derbyMeta (Connection cnc) throws Exception {
		super (cnc);
		ResultSet rs;
		Statement stmt;
		stmt = cnc.createStatement();
		rs = stmt.executeQuery ("values current schema");
		while (rs.next ()) {
			schemaAddCurrent (rs.getString (1));
		}
	}
}

