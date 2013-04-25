import java.sql.*;

// define the current schema as being "dbo"

class sqlserverMeta extends GdaJMeta {
	public sqlserverMeta (Connection cnc) throws Exception {
		super (cnc);
		schemaAddCurrent ("dbo");
	}
}

