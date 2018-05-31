import java.sql.*;
import java.util.*;
import java.io.*;

/*
 * Note:
 * This provider invokes JNI in the following way: a C API calls some Java code through JNI, which in turn calls
 * some C code as native method. To exchange pointers, the C pointers are converted to the "long" java type, which is mapped
 * to the "jlong" C type which is always 64 bits long, refer to http://docs.oracle.com/javase/1.5.0/docs/guide/jni/spec/types.html
 *
 * In C code, when passing a C pointer to a method, it needs to be converted to a jlong, and the jni_cpointer_to_jlong()
 * function must be used.
 */

/*
 * This class will be instantiated once for each GdaJdbcProvider created.
 */
class GdaJProvider {
	private static native void initIDs();
	private String driver;

	// Constructor
	public GdaJProvider (String driver) {
		this.driver = driver;
	}

	// Connection opening
	public GdaJConnection openConnection (String cnc_string, String username, String password) throws Exception {
		Connection cnc;
		try {
			Class.forName (driver);
			cnc = DriverManager.getConnection (cnc_string, username, password);
		}
		catch (Exception e) {
			throw e;
		}
		GdaJConnection jcnc = new GdaJConnection (cnc, driver);
		return jcnc;
	}

	// Get a list of all the known drivers
	// See also http://www.devx.com/tips/Tip/28818 for a list of drivers
	public static String getDrivers () {
		try {
			Class driverClass = Class.forName ("org.hsqldb.jdbcDriver");
		}
		catch (Exception e) {
			// ignore exceptions, they mean some drivers are not available
		}
		
		java.util.Enumeration e = DriverManager.getDrivers();
		String res = null;
		while (e.hasMoreElements ()) {
			Driver driver = (Driver) e.nextElement(); 
			if (res == null)
				res = driver.getClass().getName();
			else
				res = res + ":" + driver.getClass().getName();
			if (false)
				System.out.println ("FOUND DRIVER " + driver.getClass().getName());

			// Properties list, for DSN spec creation.
			if (false) {
				System.out.println("===== DEBUG: Properties for " + driver.getClass().getName());
				try {
					Properties ep = new Properties();
					DriverPropertyInfo[] props = driver.getPropertyInfo ("", ep);
					if (props != null) {
						for (int i = 0; i < props.length; i++){
							DriverPropertyInfo prop = props[i];
							System.out.println ("  Prop name = "+prop.name);
							System.out.println ("     Prop description = "+prop.description);
							System.out.println ("     Prop value = "+prop.value);
							if (prop.choices != null) {
								for (int j = 0; j < prop.choices.length; j++) {
									System.out.println ("     prop choice "+j
											    +" = "+prop.choices[j]);
								}
							}
						}
					}
				}
				catch (Exception ex) {
					ex.printStackTrace ();
				}
			}
		}
		return res;
	}
	
	public static Connection openConnection (String driver, String cnc_string) {
		System.out.println ("JAVA openConnection() requested");
		return null;
	}

	// main(): in case someone tries to exetute the JAR
	public static void main (String args[]) {
                System.out.println ("This JAR is not intended to be executed directly, " +
				    "but used as Libgda's JDBC provider!");
        }

	// class initializer
	static {
		if (System.getProperty("os.name").indexOf ("Win") >= 0) {
			// If we are running on Windows, then we need to set the full library name
			// because our library is still called libgda-jdbc.dll
			System.loadLibrary ("libgda-jdbc");
		}
		else {
			System.loadLibrary ("gda-jdbc");
		}
		initIDs ();

		try {
			/* dummy objects creation to force class loading */
			byte[] data = {'e'};
			GdaInputStream is = new GdaInputStream (data);
		}
		catch (Exception e) {
			System.out.println ("Coult not initialize some JAVA classes");
			e.printStackTrace ();
		}
	}
}


/*
 * This class will be instantiated once for each GdaConnection object
 */
class GdaJConnection {
	private static native void initIDs();
	public Connection cnc = null;
	private HashMap<String,Savepoint> svp_map = new HashMap<String,Savepoint>();
	private GdaJMeta jmeta = null;
	String driver;

	// Constructor
	public GdaJConnection (Connection cnc, String driver) throws Exception {
		this.cnc = cnc;
		this.driver = driver;
	}

	// Connection close
	public void close () throws Exception {
		this.cnc.close ();
		jmeta = null;
	}

	// get server version
	public String getServerVersion () throws Exception {
		DatabaseMetaData meta_data;
		meta_data = cnc.getMetaData ();
		return meta_data.getDatabaseProductName() + " " + meta_data.getDatabaseProductVersion ();
	}

	// prepare statement
	public GdaJPStmt prepareStatement (String sql) throws Exception {
		GdaJPStmt ps;
		PreparedStatement pstmt;

		pstmt = cnc.prepareStatement (sql);
		ps = new GdaJPStmt (pstmt);
		return ps;
	}

	// direct execution of a SELECT with no parameters
	public GdaJResultSet executeDirectSQL (String sql) throws Exception {
		Statement st;
		st = cnc.createStatement ();
		return new GdaJResultSet (st.executeQuery (sql));
	}

	// Transaction management
	public void begin () throws Exception {
		if (cnc.getAutoCommit() == false) {
			throw new Exception ("Transaction already started");
		}
		cnc.setAutoCommit (false);
	}
	
	public void rollback () throws Exception {
		if (cnc.getAutoCommit() == true) {
			throw new Exception ("No transaction started");
		}
		cnc.rollback ();
		cnc.setAutoCommit (true);
	}

	public void commit () throws Exception {
		if (cnc.getAutoCommit() == true) {
			throw new Exception ("No transaction started");
		}
		cnc.commit ();
		cnc.setAutoCommit (true);
	}

	public void addSavepoint (String svp_name) throws Exception {
		Savepoint svp;
		if (cnc.getAutoCommit() == true) {
			throw new Exception ("No transaction started");
		}

		svp = svp_map.get (svp_name);
		if (svp != null) {
			throw new Exception ("Savepoint '" + svp_name + "' already exists");
		}
		svp = cnc.setSavepoint (svp_name);
		svp_map.put (svp_name, svp);
	}

	public void rollbackSavepoint (String svp_name) throws Exception {
		Savepoint svp = null;
		if (cnc.getAutoCommit() == true) {
			throw new Exception ("No transaction started");
		}
		svp = svp_map.get (svp_name);
		if (svp == null) {
			throw new Exception ("No savepoint '" + svp_name + "' found");
		}
		cnc.rollback (svp);
		svp_map.remove (svp_name);
	}

	public void releaseSavepoint (String svp_name) throws Exception {
		Savepoint svp = null;
		if (cnc.getAutoCommit() == true) {
			throw new Exception ("No transaction started");
		}
		svp = svp_map.get (svp_name);
		if (svp == null) {
			throw new Exception ("No savepoint '" + svp_name + "' found");
		}
		cnc.releaseSavepoint (svp);
		svp_map.remove (svp_name);
	}

	// meta data retrieval
	public GdaJMeta getJMeta () throws Exception {
		if (jmeta == null) {
			String name= driver.replace (".", "_") + "Meta";
			try {
				Class<?> r = Class.forName (name);
				java.lang.reflect.Constructor c = 
					r.getConstructor (new Class [] {Class.forName ("java.sql.Connection")});
				jmeta = (GdaJMeta) c.newInstance (new Object [] {cnc});
			}
			catch (Exception e) {
				if (driver.contains ("org.apache.derby.jdbc")) {
					try {
						name = "org_apache_derbyMeta";
						Class<?> r = Class.forName (name);
						java.lang.reflect.Constructor c = 
							r.getConstructor (new Class [] {Class.forName ("java.sql.Connection")});
						jmeta = (GdaJMeta) c.newInstance (new Object [] {cnc});
					}
					catch (Exception e1) {
						// nothing
					}
				}
				else if (driver.contains ("sqlserver") ||
					 driver.contains ("com.ashna.jturbo.driver.Driver") ||
					 driver.contains ("com.inet.tds.TdsDriver")) {
					try {
						name = "sqlserverMeta";
						Class<?> r = Class.forName (name);
						java.lang.reflect.Constructor c =
							r.getConstructor (new Class [] {Class.forName ("java.sql.Connection")});
						jmeta = (GdaJMeta) c.newInstance (new Object [] {cnc});
					}
					catch (Exception e1) {
						// nothing
					}
				}
			}
			if (jmeta == null)
				jmeta = new GdaJMeta (cnc);
		}
		return jmeta;
	}

	// class initializer
	static {
		initIDs ();
	}
}

/*
 * Represents a prepared statement
 */
class GdaJPStmt {
	private static native void initIDs();
	private PreparedStatement ps;
	private Vector<GdaJValue> param_values;

	// Constructor
	public GdaJPStmt (PreparedStatement ps) {
		this.ps = ps;
		param_values = new Vector<GdaJValue> (0);
	}

	// declares the expected type for the parameters
	public void declareParamTypes (long cnc_c_pointer, byte types[]) throws Exception {
		int nparams = ps.getParameterMetaData().getParameterCount ();
		int i;
		if (types.length != nparams) {
			throw new Exception ("Number of types differs from number of parameters: got " + types.length + 
					     " and expected " + nparams);
		}

		for (i = 0; i < nparams; i++) {
			GdaJValue cv = GdaJValue.proto_type_to_jvalue (types[i]);
			cv.setGdaConnectionPointer (cnc_c_pointer);
			param_values.add (cv);
			//System.out.println ("JAVA: param " + i + " as type " + types[i]);
		}
	}

	// sets a parameter's value
	// if c_value_pointer is 0, then the parameter should be set to NULL
	public void setParameterValue (int index, long c_value_pointer) throws Exception {
		GdaJValue cv = param_values.elementAt (index);
		cv.getCValue (ps, index, c_value_pointer);
	}

	// Clear previous set parameters
	public void clearParameters () throws Exception {
		ps.clearParameters ();
	}

	// Execute
	public boolean execute () throws Exception {
		return ps.execute ();
	}

	// Fetch result set
	public GdaJResultSet getResultSet () throws Exception {
		return new GdaJResultSet (ps.getResultSet ());
	}

	public int getImpactedRows () throws Exception {
		return ps.getUpdateCount ();
	}

	// class initializer
	static {
		initIDs ();
	}
}


/*
 * This class represents meta data information for a GdaJResultSet.
 *
 * All the fields are read-only
 */
class GdaJResultSetInfos {
	private static native void initIDs();
	public Vector<GdaJColumnInfos> col_infos;
	private int ncols;
	
	// Constructor
	public GdaJResultSetInfos (ResultSetMetaData md) throws Exception {
		int i;
		ncols = md.getColumnCount ();
		col_infos = new Vector<GdaJColumnInfos> (0);
		for (i = 0; i < ncols; i++) {
			GdaJColumnInfos ci;
			int rcol = i + 1;
			ci = new GdaJColumnInfos (md.getColumnName (rcol), md.getColumnLabel (rcol), md.getColumnType (rcol));
			col_infos.add (ci);
		}
	}

	public GdaJResultSetInfos (Vector<GdaJColumnInfos> col_infos) {
		this.col_infos = col_infos;
		ncols = col_infos.size ();
	}

	// describe a column, index starting at 0
	public GdaJColumnInfos describeColumn (int col) throws Exception {
		return (GdaJColumnInfos) col_infos.elementAt (col);
	}

	// class initializer
	static {
		initIDs ();
	}
}

class GdaJColumnInfos {
	private static native void initIDs();
	String col_name;
	String col_descr;
	int col_type;

	public GdaJColumnInfos (String col_name, String col_descr, int col_type) {
		this.col_name = col_name;
		this.col_descr = col_descr;
		this.col_type = col_type;
	}

	// class initializer
	static {
		initIDs ();
	}
}

/*
 * This class represents a result set: the result of a SELECT statement
 */
class GdaJResultSet {
	private static native void initIDs();
	private ResultSet rs;

	private int ncols;
	protected Vector<GdaJValue> col_values;

	// Constructor
	public GdaJResultSet (ResultSet rs) throws Exception {
		this.rs = rs;
		if (rs != null) {
			ncols = rs.getMetaData().getColumnCount();
			col_values = new Vector<GdaJValue> (ncols);
		}
	}

	protected GdaJResultSet (int ncols) {
		this.ncols = ncols;
		col_values = new Vector<GdaJValue> (ncols);
	}

	// get result set's meta data
	public GdaJResultSetInfos getInfos () throws Exception {
		return new GdaJResultSetInfos (rs.getMetaData ());
	}

	/*
	 * declares the expected type for the column
	 */
	public void declareColumnTypes (long cnc_c_pointer, byte types[]) throws Exception {
		int i;
		if (types.length != ncols) {
			throw new Exception ("Number of types differs from number of columns: expected " + ncols + " got " +
					     types.length);
		}
		for (i = 0; i < ncols; i++) {
			GdaJValue cv = GdaJValue.proto_type_to_jvalue (types[i]);
			cv.setGdaConnectionPointer (cnc_c_pointer);
			cv.setGdaRowColumn (i);
			col_values.add (cv);
		}
		columnTypesDeclared ();
	}

	protected void columnTypesDeclared () {
		// do nothing here, let sub classes do it.
	}

	/* 
	 * move iterator to next row and fills row
	 * @c_pointer is a pointer to the new GdaRow to fill values
	 * Returns: false if no more row available
	 */
	public boolean fillNextRow (long c_pointer) throws Exception {
		int i;
		if (! rs.next ())
			return false;
		for (i = 0; i < ncols; i++) {
			GdaJValue cv = col_values.elementAt (i);
			cv.setCValue (rs, i, c_pointer);
		}
		return true;
	}

	// class initializer
	static {
		initIDs ();
	}
}


/*
 * Classes to represent value transport between the JAVA and C worlds:
 * - JAVA -> C when reading through a resultset
 * - C -> JAVA when setting the parameter's values of a prepared statement
 */
abstract class GdaJValue {
	private static native void initIDs();
	protected long cnc_c_pointer = 0;
	protected int gda_row_column = -1;
	protected boolean convert_lc = false; // true if strings must be converted to lower case (ignored by non String)
	protected boolean no_null = false; // converts a NULL value to "" for a string (ignored by non String)

	/*
	 * @c_pointer points to a GdaRow
	 * @col starts at 0
	 */
	native void setCString (long c_pointer, int col, String string);
	native void setCInt (long c_pointer, int col, int i);
	native void setCChar (long c_pointer, int col, byte b);
	native void setCDouble (long c_pointer, int col, double d);
	native void setCFloat (long c_pointer, int col, float f);
	native void setCLong (long c_pointer, int col, long l);
	native void setCShort (long c_pointer, int col, short s);
	native void setCBoolean (long c_pointer, int col, boolean b);
	native void setCDate (long c_pointer, int col, int year, int month, int day);
	native void setCTime (long c_pointer, int col, int hour, int min, int sec);
	native void setCTimestamp (long c_pointer, int col, int year, int month, int day, int hour, int min, int sec);
	native void setCBinary (long c_pointer, int col, byte[] data);
	native void setCBlob (long c_pointer, int col, long cnc_c_pointer, GdaJBlobOp blob);
	native void setCNumeric (long c_pointer, int col, String str, int precision, int scale);
	
	/*
	 * @c_pointer points to a GValue, may NOT be 0 ( <=> NULL )
	 */
	native String getCString (long c_pointer);
	native int getCInt (long c_pointer);
	native byte getCChar (long c_pointer);
	native double getCDouble (long c_pointer);
	native float getCFloat (long c_pointer);
	native long getCLong (long c_pointer);
	native short getCShort (long c_pointer);
	native boolean getCBoolean (long c_pointer);
	native java.sql.Date getCDate (long c_pointer);
	native java.sql.Time getCTime (long c_pointer);
	native java.sql.Timestamp getCTimestamp (long c_pointer);
	native byte[] getCBinary (long c_pointer);
	native GdaInputStream getCBlob (long c_pointer);
	native java.math.BigDecimal getCNumeric (long c_pointer);

	/*
	 * Sets the C GValue's value from @rs at column @col (@c_pointer points to the GdaRow
	 * which contains the GValues)
	 */
	abstract void setCValue (ResultSet rs, int col, long c_pointer) throws Exception; /* @col starts at 0 */

	/*
	 * Set @ps's parameter at index @index from the C GValue pointed by @c_pointer (if @c_pointer is 0, then
	 * the parameter is set to NULL)
	 */
	abstract void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception; /* @index starts at 0 */

	public void setGdaConnectionPointer (long cnc_c_pointer) {
		this.cnc_c_pointer = cnc_c_pointer;
	}

	public void setGdaRowColumn (int col) {
		gda_row_column = col;
	}


	// (year, month, day) -> java.sql.Date
	public static java.sql.Date createDate (int year, int month, int day) {
		Calendar cal = GregorianCalendar.getInstance();
		cal.set (year, month, day);
		return new java.sql.Date (cal.getTimeInMillis ());
	}

	// (hour, minute, sec) -> java.sql.Time
	public static java.sql.Time createTime (int hour, int min, int sec) {
		Calendar cal = GregorianCalendar.getInstance();
		cal.set (0, 0, 0, hour, min, sec);
		return new java.sql.Time (cal.getTimeInMillis ());
	}

	// (year, month, day, hour, minute, sec) -> java.sql.Timestamp
	public static java.sql.Timestamp createTimestamp (int year, int month, int day, int hour, int min, int sec) {
		Calendar cal = GregorianCalendar.getInstance();
		cal.set (year, month, day, hour, min, sec);
		return new java.sql.Timestamp (cal.getTimeInMillis ());
	}

	// types conversions
	public static GdaJValue proto_type_to_jvalue (byte type) throws Exception {
		GdaJValue cv;
		switch (type) {
		case 0: /* GDA_TYPE_NULL */
			cv = new GdaJNull ();
			break;
		case 1: /* G_TYPE_STRING */
			cv = new GdaJString ();
			break;
		case 2: /* G_TYPE_INT */
			cv = new GdaJInt ();
			break;
		case 3: /* G_TYPE_CHAR */
			cv = new GdaJChar ();
			break;
		case 4: /* G_TYPE_DOUBLE */
			cv = new GdaJDouble ();
			break;
		case 5: /* G_TYPE_FLOAT */
			cv = new GdaJFloat ();
			break;
		case 6: /* G_TYPE_BOOLEAN */
			cv = new GdaJBoolean ();
			break;
		case 7: /* G_TYPE_DATE */
			cv = new GdaJDate ();
			break;
		case 8: /* GDA_TYPE_TIME */
			cv = new GdaJTime ();
			break;
		case 9: /* GDA_TYPE_TIMESTAMP */
			cv = new GdaJTimestamp ();
			break;
		case 10: /* GDA_TYPE_BINARY */
			cv = new GdaJBinary ();
			break;
		case 11: /* GDA_TYPE_BLOB */
			cv = new GdaJBlob ();
			break;
		case 12: /* G_TYPE_INT64 */
			cv = new GdaJInt64 ();
			break;
		case 13: /* GDA_TYPE_SHORT */
			cv = new GdaJShort ();
			break;
		case 14: /* GDA_TYPE_NUMERIC */
			cv = new GdaJNumeric ();
			break;
			
		default:
			throw new Exception ("Unhandled protocol type " + type);
		}
		return cv;
	}

	// Same as gda-jdbc-recordset.c::jdbc_type_to_g_type
	// see http://docs.oracle.com/javase/6/docs/api/constant-values.html#java.sql.Types.ARRAY for reference
	public static String jdbc_type_to_g_type (int type) {
		switch (type) {
		case java.sql.Types.VARCHAR:
			return "gchararray";
		case java.sql.Types.ARRAY:
			return "GdaBinary";
		case java.sql.Types.BIGINT:
			return "gint64";
		case java.sql.Types.BINARY:
			return "GdaBinary";
		case java.sql.Types.BIT:
			return "gboolean";
		case java.sql.Types.BLOB:
			return "GdaBlob";
		case java.sql.Types.BOOLEAN:
			return "gboolean";
		case java.sql.Types.CHAR:
			return "gchararray";
		case java.sql.Types.CLOB:
		case java.sql.Types.DATALINK:
			return "GdaBinary";
		case java.sql.Types.DATE:
			return "GDate";
		case java.sql.Types.DECIMAL:
			return "GdaNumeric";
		case java.sql.Types.DISTINCT:
			return "GdaBinary";
		case java.sql.Types.DOUBLE:
			return "gdouble";
		case java.sql.Types.FLOAT:
			return "gfloat";
		case java.sql.Types.INTEGER:
			return "gint";
		case java.sql.Types.JAVA_OBJECT:
			return "GdaBinary";
		case java.sql.Types.LONGNVARCHAR:
			return "gchararray";
		case java.sql.Types.LONGVARBINARY:
			return "GdaBinary";
		case java.sql.Types.LONGVARCHAR:
			return "gchararray";
		case java.sql.Types.NCHAR:
			return "gchararray";
		case java.sql.Types.NCLOB:
			return "GdaBinary";
		case java.sql.Types.NULL:
			return "GdaNull";
		case java.sql.Types.NUMERIC:
			return "GdaNumeric";
		case java.sql.Types.NVARCHAR:
			return "gchararray";
		case java.sql.Types.OTHER:
			return "GdaBinary";
		case java.sql.Types.REAL:
			return "gfloat";
		case java.sql.Types.REF:
			return "GdaBinary";	
		case java.sql.Types.ROWID:
			return "gchararray";
		case java.sql.Types.SMALLINT:
			return "GdaShort";
		case java.sql.Types.SQLXML:
			return "gchararray";
		case java.sql.Types.STRUCT:
			return "GdaBinary";
		case java.sql.Types.TIME:
			return "GdaTime";
		case java.sql.Types.TIMESTAMP:
			return "GDateTime";
		case java.sql.Types.TINYINT:
			return "gchar";
		case java.sql.Types.VARBINARY:
		default:
			return "GdaBinary";
		}
	}

	public static String toLower (String string) {
		String s2 = string.toUpperCase();
		if (s2 == string)
			return string.toLowerCase();
		else
			return string;
	}

	// class initializer
	static {
		initIDs ();
	}
}

class GdaJString extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		String string;
		string = rs.getString (col + 1);
		if (string == null) {
			if (no_null)
				setCString (c_pointer, gda_row_column, "");
		}
		else {
			if (convert_lc) {
				String s2 = string.toUpperCase();
				if (s2 == string)
					string = string.toLowerCase();
			}
			setCString (c_pointer, gda_row_column, string);
		}
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.VARCHAR);
		else {
			String string;
			string = getCString (c_pointer);
			ps.setString (index + 1, string);
		}
	}
}

class GdaJInt extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		int i;
		i = rs.getInt (col + 1);
		if ((i != 0) || !rs.wasNull ())
			setCInt (c_pointer, gda_row_column, i);
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.INTEGER);
		else {
			int i;
			i = getCInt (c_pointer);
			ps.setInt (index + 1, i);
		}
	}
}

class GdaJDouble extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		double d;
		d = rs.getDouble (col + 1);
		if ((d != 0) || !rs.wasNull ())
			setCDouble (c_pointer, gda_row_column, d);
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.DOUBLE);
		else {
			double d;
			d = getCDouble (c_pointer);
			ps.setDouble (index + 1, d);
		}
	}
}

class GdaJFloat extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		float f;
		f = rs.getFloat (col + 1);
		if ((f != 0) || !rs.wasNull ())
			setCFloat (c_pointer, gda_row_column, f);
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.FLOAT);
		else {
			float f;
			f = getCFloat (c_pointer);
			ps.setFloat (index + 1, f);
		}
	}
}

class GdaJBoolean extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		boolean b;
		b = rs.getBoolean (col + 1);
		if (b || !rs.wasNull ())
			setCBoolean (c_pointer, gda_row_column, b);
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.BOOLEAN);
		else {
			boolean b;
			b = getCBoolean (c_pointer);
			ps.setBoolean (index + 1, b);
		}
	}
}

class GdaJDate extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		java.sql.Date date;
		date = rs.getDate (col + 1);
		if (date != null) {
			Calendar cal = GregorianCalendar.getInstance();
			cal.setTime (date);
			setCDate (c_pointer, gda_row_column, cal.get (Calendar.YEAR), 
				  cal.get (Calendar.MONTH) + 1, cal.get (Calendar.DAY_OF_MONTH));
		}
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.DATE);
		else {
			java.sql.Date d;
			d = getCDate (c_pointer);
			ps.setDate (index + 1, d);
		}
	}
}

class GdaJTime extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		java.sql.Time time;
		time = rs.getTime (col + 1);
		if (time != null) {
			Calendar cal = GregorianCalendar.getInstance();
			cal.setTime (time);
			setCTime (c_pointer, gda_row_column, cal.get (Calendar.HOUR_OF_DAY), 
				  cal.get (Calendar.MINUTE) + 1, cal.get (Calendar.SECOND));
		}
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.TIME);
		else {
			java.sql.Time t;
			t = getCTime (c_pointer);
			ps.setTime (index + 1, t);
		}
	}
}

class GdaJTimestamp extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		java.sql.Timestamp ts;
		ts = rs.getTimestamp (col + 1);
		if (ts != null) {
			Calendar cal = GregorianCalendar.getInstance();
			cal.setTime (ts);
			setCTimestamp (c_pointer, gda_row_column, cal.get (Calendar.YEAR), 
				       cal.get (Calendar.MONTH) + 1, cal.get (Calendar.DAY_OF_MONTH),
				       cal.get (Calendar.HOUR_OF_DAY), 
				       cal.get (Calendar.MINUTE) + 1, cal.get (Calendar.SECOND));
		}
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.TIMESTAMP);
		else {
			java.sql.Timestamp ts;
			ts = getCTimestamp (c_pointer);
			ps.setTimestamp (index + 1, ts);
		}
	}
}

class GdaJBinary extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		byte[] bin;
		bin = rs.getBytes (col + 1);
		if (bin != null) {
			setCBinary (c_pointer, gda_row_column, bin);
		}
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.BINARY);
		else {
			byte[] bin;
			bin = getCBinary (c_pointer);
			ps.setBytes (index + 1, bin);
		}
	}
}

class GdaJBlob extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		java.sql.Blob blob;
		blob = rs.getBlob (col + 1);
		if (blob != null) {
			setCBlob (c_pointer, gda_row_column, cnc_c_pointer, new GdaJBlobOp (blob));
		}
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.BLOB);
		else {
			GdaInputStream is;
			is = getCBlob (c_pointer);
			ps.setBinaryStream (index + 1, is, (int) is.size);
		}
	}
}

class GdaJChar extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		Byte b;
		b = rs.getByte (col + 1);
		if ((b != 0) || !rs.wasNull ())
			setCChar (c_pointer, gda_row_column, b);
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.CHAR);
		else {
			byte b;
			b = getCChar (c_pointer);
			ps.setByte (index + 1, b);
		}
	}
}

class GdaJInt64 extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		long l;
		l = rs.getLong (col + 1);
		if ((l != 0) || !rs.wasNull ())
			setCLong (c_pointer, gda_row_column, l);
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.BIGINT);
		else {
			long l;
			l = getCLong (c_pointer);
			ps.setLong (index + 1, l);
		}
	}
}

class GdaJShort extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		short s;
		s = rs.getShort (col + 1);
		if ((s != 0) || !rs.wasNull ())
			setCShort (c_pointer, gda_row_column, s);
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.SMALLINT);
		else {
			short s;
			s = getCShort (c_pointer);
			ps.setShort (index + 1, s);
		}
	}
}

class GdaJNumeric extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		java.math.BigDecimal bd;

		bd = rs.getBigDecimal (col + 1);
		if (bd != null) {
			setCNumeric (c_pointer, gda_row_column, bd.toString(), bd.precision(), bd.scale());
		}
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		if (c_pointer == 0)
			ps.setNull (index + 1, java.sql.Types.NUMERIC);
		else {
			java.math.BigDecimal bd;
			bd = getCNumeric (c_pointer);
			ps.setBigDecimal (index + 1, bd);
		}
	}
}

class GdaJNull extends GdaJValue {
	void setCValue (ResultSet rs, int col, long c_pointer) throws Exception {
		// nothing to do
	}

	void getCValue (PreparedStatement ps, int index, long c_pointer) throws Exception {
		ParameterMetaData md;
		int i = index + 1;
		md = ps.getParameterMetaData ();
		ps.setNull (i, md.getParameterType (i));
	}
}


/*
 * This class represents a Blob, used by the GdaBlobOp object
 * to transfer data from JDBC -> C
 */
class GdaJBlobOp {
	private static native void initIDs();
	private Blob blob;

	// Constructor
	public GdaJBlobOp (Blob blob) {
		this.blob = blob;
	}

	// Get Blob's length
	public long length () throws Exception {
		return blob.length ();
	}

	// Read data
	public byte[] read (long offset, int len) throws Exception {
		return blob.getBytes (offset + 1, len);
	}

	// Write data
	public int write (long offset, byte[] data) throws Exception {
		int i;
		i = blob.setBytes (offset, data);
		if (i != data.length) {
			throw new Exception ("Could not write the complete BLOB");
		}
		return i;
	}

	// class initializer
	static {
		initIDs ();
	}
}

/*
 * an InputStream which reads from a GdaBlob, for
 * data transfer C -> JDBC
 *
 * Note: the @size attribute is not used here since the PreparedStatement.setBinaryStream() already
 * contains the blob's size and won't try to get more. If this InputStream were to be used
 * somewhere else, then @size would have to be handled correctly.
 */
class GdaInputStream extends InputStream {
	static int chunk_size = 65536; // 64kb chunks

	private static native void initIDs();
	private native byte[] readByteData (long gda_blob_pointer, long offset, long size);
	public long size;
	private long current_pos;
	private long gda_blob_pointer;

	private InputStream ist = null; // delegate

	// Constructor
	public GdaInputStream (long gda_blob_pointer, long size) {
		current_pos = 0;
		this.size = size;
		this.gda_blob_pointer = gda_blob_pointer;
	}

	// Constructor
	public GdaInputStream (byte[] data) {
		this.gda_blob_pointer = 0;
		ist = new ByteArrayInputStream (data);
	}

	// abstract class implementation
	public int read() throws IOException {
		int i = -1;
		if (ist != null)
			i = ist.read ();

		if ((i == -1) && (gda_blob_pointer != 0)) {
			byte[] data;
			data = readByteData (gda_blob_pointer, current_pos, chunk_size);
			current_pos += data.length;
			ist = new ByteArrayInputStream (data);
			i = ist.read ();
		}
		return i;
	}

	// class initializer
	static {
		initIDs ();
	}
}
