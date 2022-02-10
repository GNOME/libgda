namespace Gda {
	[CCode (cheader_filename = "libgda/libgda.h", has_type_id = false)]
	[Compact]
	public abstract class BlobOpFunctions {
		public abstract long get_length (BlobOp op);
		public abstract long read (BlobOp op, Blob blob, long offset, long size);
		public abstract long write (BlobOp op, Blob blob, long offset);
		public abstract bool write_all (BlobOp op, Blob blob);
	}

	[CCode (cheader_filename = "libgda/libgda.h", has_type_id = false)]
	[Compact]
	public abstract class ServerProviderBase {
		public abstract string get_name (ServerProvider provider);
		public abstract string get_version (ServerProvider provider);
		public abstract string get_server_version (ServerProvider provider, Connection cnc);
		public abstract bool supports_feature (ServerProvider provider, Connection cnc,
								 ConnectionFeature feature);
		public abstract Worker create_worker (ServerProvider provider, bool for_cnc);
		public abstract Connection create_connection (ServerProvider provider);
		public abstract SqlParser create_parser (ServerProvider provider, Connection cnc);
		public abstract DataHandler get_data_handler (ServerProvider provider, Connection? cnc,
								 GLib.Type g_type, string dbms_type);
		public abstract string get_def_dbms_type (ServerProvider provider, Connection cnc, GLib.Type  g_type);
		public abstract bool supports_operation (ServerProvider provider, Connection cnc,
								 ServerOperationType type, Set options);
		public abstract ServerOperation create_operation (ServerProvider provider, Connection cnc,
								 ServerOperationType type, Set options) throws GLib.Error;
		public abstract string render_operation (ServerProvider provider, Connection cnc,
								 ServerOperation op) throws GLib.Error;
		public abstract string statement_to_sql (ServerProvider provider, Connection cnc,
								 Statement stmt, Set params, StatementSqlFlag flags,
								 ref GLib.SList params_used) throws GLib.Error;
		public abstract string identifier_quote (ServerProvider provider, Connection? cnc,
								 string id,
								 bool for_meta_store, bool force_quotes);

		public abstract SqlStatement statement_rewrite (ServerProvider provider, Connection cnc,
								 Statement stmt, Set params) throws GLib.Error;
		public abstract bool open_connection (ServerProvider provider, Connection cnc,
							 QuarkList params, QuarkList auth);
		public abstract bool prepare_connection (ServerProvider provider, Connection cnc,
							 QuarkList params, QuarkList auth);
		public abstract bool close_connection (ServerProvider provider, Connection cnc);
		public abstract string escape_string (ServerProvider provider, Connection cnc, string str);
		public abstract string unescape_string (ServerProvider provider, Connection cnc, string str);
		public abstract bool perform_operation (ServerProvider provider, Connection? cnc,
							 ServerOperation op) throws GLib.Error;
		public abstract bool begin_transaction (ServerProvider provider, Connection cnc,
							 string name, TransactionIsolation level) throws GLib.Error;
		public abstract bool commit_transaction (ServerProvider provider, Connection cnc,
							 string name) throws GLib.Error;
		public abstract bool rollback_transaction (ServerProvider provider, Connection cnc,
							 string name) throws GLib.Error;
		public abstract bool add_savepoint (ServerProvider provider, Connection cnc,
							 string name) throws GLib.Error;
		public abstract bool rollback_savepoint (ServerProvider provider, Connection cnc,
							 string name) throws GLib.Error;
		public abstract bool delete_savepoint (ServerProvider provider, Connection cnc,
							 string name) throws GLib.Error;
		public abstract bool statement_prepare (ServerProvider provider, Connection cnc,
							 Statement stmt) throws GLib.Error;
		public abstract GLib.Object statement_execute (ServerProvider provider, Connection cnc,
							 Statement stmt, Set params,
							 StatementModelUsage model_usage,
							 GLib.Type[] col_types, ref Set last_inserted_row) throws GLib.Error;

		/*< private >*/
		/* Padding for future expansion */
		public abstract void _gda_reserved11 ();
		public abstract void _gda_reserved12 ();
		public abstract void _gda_reserved13 ();
		public abstract void _gda_reserved14 ();
	}

	[CCode (cheader_filename = "libgda/libgda.h", has_type_id = false)]
	[Compact]
	public abstract class SqlStatementContentsInfo {
		public Gda.SqlStatementType type;
		public weak string name;
		public abstract void* construct ();
		public abstract void free (void* stm);
		public abstract void* copy (void* stm);
		public abstract string serialize (void* stm);
		public weak Gda.SqlForeachFunc check_structure_func;
		public weak Gda.SqlForeachFunc check_validity_func;
		public abstract void* _gda_reserved1 ();
		public abstract void* _gda_reserved2 ();
		public abstract void* _gda_reserved3 ();
		public abstract void* _gda_reserved4 ();
	}
	[CCode (cheader_filename = "libgda.h", copy_function = "g_boxed_copy", free_function = "g_boxed_free", type_id = "gda_sql_statement_get_type ()")]
	[Compact]
	public class SqlStatement {
		#if VALA_0_50
		#else
		public static Gda.SqlStatementContentsInfo get_contents_infos (Gda.SqlStatementType type);
		#endif
	}

}
