unit GdaConnection;

  interface

{  uses
    glib,Gdk,Gtk,
    orbit,basic_types;
}

   uses
    glib,gtkobjects,extern_types,gdarecordset;

   const
      gdafile = 'gda-client';

   type
     GDA_Connection_QType = longint;
   const
      GDCN_SCHEMA_AGGREGATES	    = 0;
      GDCN_SCHEMA_ASSERTS	    = 1;
      GDCN_SCHEMA_CATALOGS	    = 2;
      GDCN_SCHEMA_CHARSETS          = 3;
      GDCN_SCHEMA_CHECK_CONSTRAINTS = 4;
      GDCN_SCHEMA_COLLATIONS	    = 5;
      GDCN_SCHEMA_COL_DOM_USAGE     = 6;
      GDCN_SCHEMA_COL_PRIVS         = 7;
      GDCN_SCHEMA_COLS	            = 8;
      GDCN_SCHEMA_CONSTR_COL_USAGE  = 9;
      GDCN_SCHEMA_CONSTR_TAB_USAGE  = 10;
      GDCN_SCHEMA_FOREIGN_KEYS      = 11;
      GDCN_SCHEMA_INDEXES	    = 12;
      GDCN_SCHEMA_INDEX_COLS	    = 13;
      GDCN_SCHEMA_KEY_COL_USAGE     = 14;
      GDCN_SCHEMA_PRIM_KEYS	    = 15;
      GDCN_SCHEMA_PROC_COLUMNS      = 16;
      GDCN_SCHEMA_PROC_PARAM        = 17;
      GDCN_SCHEMA_PROCS	            = 18;
      GDCN_SCHEMA_PROV_SPEC	    = 19;
      GDCN_SCHEMA_PROV_TYPES	    = 20;
      GDCN_SCHEMA_REF_CONSTRAINTS   = 21;
      GDCN_SCHEMA_SEQUENCE          = 22;
      GDCN_SCHEMA_SQL_LANG          = 23;
      GDCN_SCHEMA_STATISTICS        = 24;
      GDCN_SCHEMA_TAB_CONSTRAINTS   = 25;
      GDCN_SCHEMA_TAB_PARENTS       = 26;
      GDCN_SCHEMA_TAB_PRIVS         = 27;
      GDCN_SCHEMA_TABLES            = 28;
      GDCN_SCHEMA_TRANSLATIONS      = 29;
      GDCN_SCHEMA_USAGE_PROVS       = 30;
      GDCN_SCHEMA_VIEW_COL_USAGE    = 31;
      GDCN_SCHEMA_VIEW_TAB_USAGE    = 32;
      GDCN_SCHEMA_VIEWS	            = 33;
      GDCN_SCHEMA_LAST	            = 34;

   type
      GDA_Connection_Feature = Longint;
   const
      FEATURE_TRANSACTIONS = 0;
      FEATURE_SEQUENCES    = 1;
      FEATURE_PROCS	   = 2;
      FEATURE_INHERITANCE  = 3;
      FEATURE_OBJECT_ID    = 4;
      FEATURE_XML_QUERIES  = 5;

   type
     Gda_Connection = record
          pobject : PGtkObject;
          connection : CORBA_Object;
	  orb : CORBA_ORB;
          commands : pGList;
          recordsets : pGList;
          provider : ^char;
          default_db : ^char;
          database : ^char;
          user : ^char;
          passwd : ^char;
          errors_head : ^char;
          is_open : Integer;
       end;

     pGda_Connection = ^Gda_Connection;

     Gda_Connection_Class = record
          parent_class : PGtkObjectClass;
          error : procedure (para1:pGda_Connection; para2:pGList);
          warning : procedure (para1:pGda_Connection; para2:pGList);
		    open : procedure (para1:pGda_Connection);
          close : procedure (para1:pGda_Connection);
       end;

     Gda_Constraint_Element = record
          ptype : GDA_Connection_QType;
          value : ^char;
       end;

		  
		  
  function gda_connection_get_type:Integer; cdecl; external gdafile name 'gda_connection_get_type';

  function gda_connection_new(orb:CORBA_ORB):pGda_Connection; cdecl; external gdafile name 'gda_connection_new';

  procedure gda_connection_free(cnc:pGda_Connection); cdecl; external gdafile name 'gda_connection_free';

  function gda_connection_list_providers:pGList; cdecl; external gdafile name 'gda_connection_list_providers';

  procedure gda_connection_set_provider(cnc:pGda_Connection; name:pChar); cdecl; external gdafile name 'gda_connection_set_provider';

  function gda_connection_get_provider(cnc:pGda_Connection):pChar; cdecl; external gdafile name 'gda_connection_get_provider';

  function gda_connection_supports(cnc:pGda_Connection; feature:GDA_Connection_Feature):Boolean; cdecl; external gdafile name 'gda_connection_supports';

  procedure gda_connection_set_default_db(cnc:pGda_Connection; dsn:pChar); cdecl; external gdafile name 'gda_connection_set_default_db';

  function gda_connection_open(cnc:pGda_Connection; dsn:pChar; user:pChar; pwd:pChar):Integer; cdecl; external gdafile name 'gda_connection_open';

  procedure gda_connection_close(cnc:pGda_Connection); cdecl; external gdafile name 'gda_connection_close';

  function gda_connection_open_schema(cnc:pGda_Connection; t:GDA_Connection_QType;args:array of const):pGda_Recordset; cdecl; external gdafile name 'gda_connection_open_schema'; { *MIKI* faltan parámetros opcionales }

  function gda_connection_open_schema_array(cnc : pGda_Connection; t:GDA_Connection_QType;para3:Gda_Constraint_Element):pGda_Recordset; cdecl; external gdafile name 'gda_connection_open_schema_array';

  function gda_connection_get_errors(cnc:pGda_Connection):pGList; cdecl; external gdafile name 'gda_connection_get_errors';

  function gda_connection_list_datasources(cnc:pGda_Connection):pGList; cdecl; external gdafile name 'gda_connection_list_datasources';

  function gda_connection_begin_transaction(cnc:pGda_Connection):Integer; cdecl; external gdafile name 'gda_connection_begin_transaction';

  function gda_connection_commit_transaction(cnc:pGda_Connection):Integer; cdecl; external gdafile name 'gda_connection_commit_transaction';

  function gda_connection_rollback_transaction(cnc:pGda_Connection):Integer; cdecl; external gdafile name 'gda_connection_rollback_transaction';

  function gda_connection_execute(cnc:pGda_Connection; txt:pChar; reccount:pgulong; flags:gulong):pGda_Recordset; cdecl; external gdafile name 'gda_connection_execute';

  function gda_connection_start_logging(cnc:pGda_Connection; filename:pChar):Integer; cdecl; external gdafile name 'gda_connection_start_logging';

  function gda_connection_stop_logging(cnc:pGda_Connection):Integer; cdecl; external gdafile name 'gda_connection_stop_logging';

  function gda_connection_create_recordset(cnc:pGda_Connection; rs:pGda_Recordset):pChar; cdecl; external gdafile name 'gda_connection_create_recordset';

{ *MIKI*
  function gda_connection_corba_exception(cnc:pGda_Connection; ev:pCORBA_Environment):Integer; cdecl; external gdafile name 'gda_connection_corba_exception';

  procedure gda_connection_add_single_error(cnc:pGda_Connection; error:pGda_Error); cdecl; external gdafile name 'gda_connection_add_single_error';
}
  procedure gda_connection_add_errorlist(cnc:pGda_Connection; list:pGList); cdecl; external gdafile name 'gda_connection_add_errorlist';

  function gda_connection_is_open(cnc:pGda_Connection) : Integer; cdecl; external gdafile name 'gda_connection_is_open';
{
  function gda_connection_get_dsn(cnc:pGda_Connection) : pCorbaObject; cdecl; external gdafile name 'gda_connection_get_dsn';
}
  function gda_connection_get_user(cnc:pGda_Connection) : pChar; cdecl; external gdafile name 'gda_connection_get_user';

  function gda_connection_get_password(cnc:pGda_Connection) : pChar; cdecl; external gdafile name 'gda_connection_get_password';

  function gda_connection_get_flags(cnc:pGda_Connection):longint; cdecl; external gdafile name 'gda_connection_get_flags';

  procedure gda_connection_set_flags(cnc:pGda_Connection; flags:longint); cdecl; external gdafile name 'gda_connection_set_flags';

  function gda_connection_get_cmd_timeout(cnc:pGda_Connection):longint; cdecl; external gdafile name 'gda_connection_get_cmd_timeout';

  procedure gda_connection_set_cmd_timeout(cnc:pGda_Connection; cmd_timeout:longint); cdecl; external gdafile name 'gda_connection_get_cmd_timeout';

  function gda_connection_get_connect_timeout(cnc:pGda_Connection):longint; cdecl; external gdafile name 'gda_connection_get_connect_timeout';

  procedure gda_connection_set_connect_timeout(cnc:pGda_Connection; timeout:longint); cdecl; external gdafile name 'gda_connection_set_connect_timeout';
{ *MIKI*
  function gda_connection_get_cursor_location(cnc:pGda_Connection):GDA_CursorLocation; cdecl; external gdafile name 'gda_connection_get_cursor_location';

  procedure gda_connection_set_cursor_location(cnc:pGda_Connection; cursor:GDA_CursorLocation); cdecl; external gdafile name 'gda_connection_set_cursor_location';
}
  function gda_connection_get_version(cnc:pGda_Connection):pChar; cdecl; external gdafile name 'gda_connection_get_version';

  implementation

end.
