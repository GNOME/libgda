<?php
/* Configure session cache */
session_cache_limiter('nocache');
session_start();

header('Content-type: text/plain; charset=UTF-8');

include_once "gda-utils.php";
include_once "gda-exception.php";
include_once "gda-meta.php";
include_once "gda-config.php";

$debug = false;

if (! try_include ("MDB2.php")) {
	$reply = new SimpleXMLElement("<reply></reply>");
	$node = $reply->addChild ("status", "CLOSED");
	$node->addAttribute ("error", "The PEAR MDB2 extension is required");
	echo gda_add_hash ($init_shared, $reply->asXml());
	session_destroy ();
	exit (1);
}

if (! extension_loaded ("SimpleXML")) {
	$reply = new SimpleXMLElement("<reply></reply>");
	$node = $reply->addChild ("status", "CLOSED");
	$node->addAttribute ("error", "The SimpleXML extension is required");
	echo $reply->asXml();
	exit (1);
}

register_shutdown_function ('shutdown');

$cmdfile = get_command_filename (session_id ());
$replyfile = get_reply_filename (session_id ());

if (isset ($_SESSION ["cncclosed"]) && $_SESSION ["cncclosed"]) {
	/* just wait a little for the worker to terminate and return:
	 * the worker script has been re-spawned but should not try to read
	 * any further command 
	 */
	usleep (50000);
}
else {
	if (isset ($_SESSION ["counter"]))
		$_SESSION ["counter"] ++;
	else
		$_SESSION ["counter"] = 1;
	if (isset ($log))
		$log->lwrite ("WORKER started");
	gda_worker_main ();
}

function shutdown ()
{
	if (isset ($log))
		$log->lwrite ("SHUTDOWN");
	session_write_close ();
	flush ();
	ob_flush();
}

function send_reply ($retval)
{
	global $cmdfile;
	global $replyfile;
	global $debug;

	if ($debug) {
		echo "{{{".$retval."}}}\n";
		flush ();
		ob_flush();
	}

	if (!file_exists ($replyfile))
		throw new GdaException ("Bad setup", true);

	$datafile = tempnam (session_save_path (), "GDAReply");
	if (file_put_contents ($datafile, $retval) == false)
		throw new GdaException ("Can't send reply ", true);
	
	$file = fopen ($replyfile, "w");
	if (fwrite ($file, $datafile) == false)
		throw new Exception ("Can't send command");
	fclose ($file);
}

/*
 * Main function, leaves only when a database error occurs of when
 * asked to.
 */
function gda_worker_main ()
{
	global $init_shared;
	global $cmdfile;
	global $replyfile;

	$reply = new SimpleXMLElement("<reply></reply>");
	$reply->addChild ('session', htmlspecialchars(SID));
	$reply->addChild ('status', "OK");
	$reply->addChild ("counter", $_SESSION ["counter"]);
	echo gda_add_hash ($init_shared, $reply->asXml());
	flush (); ob_flush();

	/* set max execution time to 10s */
	set_time_limit (10);

	while (1) {
		$doquit = false;
		try {
			if (!file_exists ($cmdfile))
				throw new GdaException ("Bad setup", true);
			//echo "Reading from fifo...\n"; flush (); ob_flush();
			$file = fopen ($cmdfile, "r+");
			stream_set_blocking ($file, false);
			while (1) {
				$readers = array ($file);
				if (stream_select ($readers, $writers=null, $except=null, 5, 0) < 1) {
					/* More waiting... */
					echo chr(0); flush (); ob_flush(); // send something to the client to test connection
					if (connection_status() != 0) {
						fclose ($file);
						exit (0);
						break;
					}
					continue;
				}
				else {
					/* data available to read */
					break;
				}
			}
			$datafile = fread ($file, 8192);
			fclose ($file);
			
			$text = file_get_contents ($datafile);
			@unlink ($datafile);
			if ($text == false)
				throw new GdaException('Could not read command', false);

			//echo "COMMAND [$datafile]\n"; flush (); ob_flush();

			$array = split ("\n", $text, 2);
			if (sizeof ($array) != 2)
				throw new GdaException('Missing message hash', true);
			if ($array[0][0] == "<")
				throw new GdaException('Bad message hash', true);
			if ($array[0] != "NOHASH" ||
			    isset ($_SESSION['key'])) {
				$hash = hmac ($_SESSION['key'], $array[1]);
				if ($array[0] != $hash)
					throw new GdaException('Bad message hash', true);
			}
			if ($array[1][0] != "<")
				throw new GdaException('Bad message format', true);
			
			$xml = simplexml_load_string ($array[1]);
			$retval = gda_handle_xml ($xml, $doquit);

			/* send reply */
			try {
				send_reply ($retval);
			}
			catch (Exception $e) {
				/* will be caught by the listening thread */
				echo $retval;
			}
		}
		catch (GdaException $e) {
			$reply = new SimpleXMLElement("<reply></reply>");
			if (! $e->cnc_closed) {
				$chal = rand_str ();
				$_SESSION['challenge'] = $chal;
				$reply->addChild ('challenge', $chal);
				
				if (isset ($debug) && $debug)
					$reply->addChild ('session', htmlspecialchars(SID));

				$node = $reply->addChild ('status', "ERROR");
			}
			else {
				$_SESSION ["cncclosed"] = true;
				$doquit = true;
				$node = $reply->addChild ('status', "CLOSED");
			}
			$node->addAttribute ("error", $e->getMessage());

			$reply->addChild ("counter", $_SESSION ["counter"]);

			if (isset ($_SESSION['key']))
				$retval = gda_add_hash ($_SESSION['key'], $reply->asXml());
			else
				$retval = gda_add_hash ("", $reply->asXml());

			try {
				send_reply ($retval);
			}
			catch (Exception $e) {
				/* will be caught by the listening thread */
				echo $retval;
			}
		}
		//echo "REPLY [$retval]\n";
		flush ();
		ob_flush();
		if ($doquit)
			break;
	}
}

function handle_pear_error ($res, $reply)
{
	if (PEAR::isError($res)) {
		global $debug;
		if (isset ($debug) && $debug) {
			$cause = "Standard Message [".$res->getMessage()."] ". 
				"User Information  [".$res->getUserInfo()."] ".
				"Debug Information [".$res->getDebugInfo()."]";
			$reply->addChild ("debug", $cause);
		}
		$tmp = $res->getMessage ();
		$tmp = str_replace ("MDB2 Error: ", "", $tmp);
		throw new GdaException ($tmp, false);
	}
}

function determine_db_type ($mdb2)
{
	if (strstr (get_class ($mdb2), "pgsql"))
		return "PostgreSQL";
	else if (strstr (get_class ($mdb2), "mysql"))
		return "MySQL";
	else if (strstr (get_class ($mdb2), "sqlite"))
		return "SQLite";
	else if (strstr (get_class ($mdb2), "oci"))
		return "Oracle";

	return false;
}

/* MDB2 connection, global so it can persist multiple gda_handle_xml() calls */
$mdb2 = null;
$prepared_statements = array();

function real_connect ($dsn, $reply)
{
	global $mdb2;
	$options = array(
			 'debug'       => 2,
			 'portability' => MDB2_PORTABILITY_DELETE_COUNT | MDB2_PORTABILITY_EMPTY_TO_NULL |
			 MDB2_PORTABILITY_ERRORS | MDB2_PORTABILITY_FIX_ASSOC_FIELD_NAMES | MDB2_PORTABILITY_NUMROWS |
			 MDB2_PORTABILITY_RTRIM,
			 );
	$mdb2 = &MDB2::connect ($dsn."?new_link=true", $options);
	handle_pear_error ($mdb2, $reply);

	/* load extra modules */
	$mdb2->loadModule('Reverse', null, true);
	$mdb2->loadModule('Manager', null, true);
	$mdb2->loadModule('Datatype', null, true);

	return $mdb2;
}

function gda_handle_xml ($xml, &$doquit)
{
	//var_dump($xml);
	$reply = new SimpleXMLElement("<reply></reply>");
	$apply_md5_to_key = false;
	$doquit = false;
		
	global $debug;
	global $init_shared;
	global $cnc;
	global $dsn;
	global $mdb2;
	$token = "-";
	$status = "";

	if ($xml->getName() != "request")
		throw new GdaException('Bad XML input', true);
	foreach ($xml->children() as $child) {
		if ($child->getName() == "token") {
			$token = $child[0];
			if ($_SESSION['key'] == $init_shared) {
				/* change $_SESSION['key'] depending on the configured connections */
				foreach ($cnc as $dbname => $dbpass) {
					$computed_token = hmac ($dbname."/AND/".$dbpass, $_SESSION['challenge']);
					if ($computed_token == $token) {
						$_SESSION['key'] = $dbname."/AND/".$dbpass;
						$_SESSION['dsn'] = $dsn[$dbname];
						$cncfound = true;
						break;
					}
				}
				if (!isset ($cncfound))
					throw new GdaException('Connection not found', true);
			}
			else {
				$computed_token = hmac ($_SESSION['key'], $_SESSION['challenge']);
				if ($computed_token != $token)
					throw new GdaException('Authentication error', true);
			}
		}
		else if ($child->getName() == "cmd") {
			switch ($child[0]) {
			case "HELLO":
				/* just output a new challenge */
				$_SESSION['key'] = $init_shared;
				$reply->addChild ('session', htmlspecialchars(SID));
				$status = "OK";
				break;

			case "CONNECT":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);
					
				/* actually open the connection */
				$mdb2 = real_connect ($_SESSION['dsn'], $reply);			       

				$apply_md5_to_key = true;
				$dbtype = determine_db_type ($mdb2);
				if ($dbtype)
					$reply->addChild ('servertype', $dbtype);
				$reply->addChild ('serverversion', $mdb2->getServerVersion (true));
				$status = "OK";
				break;

			case "BYE":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);

				/* actually close the connection */
				if (isset ($mdb2))
					$mdb2->disconnect();

				$status = "CLOSED";
				$doquit = true;
				$_SESSION ["cncclosed"] = true;
				break;

			case "PREPARE":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);
				$argsnode = null;
				$type_is_result = false;
				foreach ($child->children() as $sqlnode) {
					if ($sqlnode->getName() == "sql") {
						$sql = $sqlnode[0];
						foreach($sqlnode->attributes() as $a => $b) {
							if (($a == "type") && ($b == "SELECT")) {
								$type_is_result = true;
								break;
							}
						}
					}
					else if ($sqlnode->getName() == "arguments") {
						$argsnode = $sqlnode;
					}
				}
				if (! isset ($sql))
					throw new GdaException('Bad XML input', true);

				/* actually execute SQL in $sql */
				if (!isset ($mdb2))
					$mdb2 = real_connect ($_SESSION['dsn'], $reply);

				do_prepare ($reply, $mdb2, $sql, $type_is_result, $argsnode);
				$status = "OK";
				break;
			case "UNPREPARE":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);
				$pstmt_hash = null;
				foreach ($child->children() as $sqlnode) {
					if ($sqlnode->getName() == "preparehash") {
						$pstmt_hash = $sqlnode[0];
						break;
					}
				}
				/* actually execute SQL in $sql */
				if (isset ($mdb2) && isset ($pstmt_hash))
					do_unprepare ($reply, $mdb2, $pstmt_hash);
				$status = "OK";
				break;
			case "EXEC":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);
				$argsnode = null;
				$pstmt_hash = null;
				$type_is_result = false;

				foreach ($child->children() as $sqlnode) {
					if ($sqlnode->getName() == "sql") {
						$sql = $sqlnode[0];
						foreach ($sqlnode->attributes() as $a => $b) {
							if (($a == "type") && ($b == "SELECT")) {
								$type_is_result = true;
								break;
							}
						}
					}
					else if ($sqlnode->getName() == "arguments")
						$argsnode = $sqlnode;
					else if ($sqlnode->getName() == "preparehash")
						$pstmt_hash = $sqlnode[0];
				}
				if (! isset ($sql))
					throw new GdaException('Bad XML input', true);
					
				/* actually execute SQL in $sql */
				if (!isset ($mdb2))
					$mdb2 = real_connect ($_SESSION['dsn'], $reply);

				do_exec ($reply, $mdb2, $pstmt_hash, $sql, $type_is_result, $argsnode);
				$status = "OK";
				break;
			case "BEGIN":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);
					
				/* actually execute SQL in $sql */
				if (!isset ($mdb2))
					$mdb2 = real_connect ($_SESSION['dsn'], $reply);

				foreach ($child->attributes() as $a => $b) {
					if ($a == "svpname") {
						$svpname = $b;
						break;
					}
				}
				if (isset ($svpname))
					$res = $mdb2->beginTransaction ($svpname);
				else
					$res = $mdb2->beginTransaction ();
				handle_pear_error ($res, $reply);
				$status = "OK";
				break;
			case "COMMIT":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);
					
				/* actually execute SQL in $sql */
				if (!isset ($mdb2))
					$mdb2 = real_connect ($_SESSION['dsn'], $reply);

				$res = $mdb2->commit ();
				handle_pear_error ($res, $reply);
				$status = "OK";
				break;
			case "ROLLBACK":
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);
					
				/* actually execute SQL in $sql */
				if (!isset ($mdb2))
					$mdb2 = real_connect ($_SESSION['dsn'], $reply);

				foreach ($child->attributes() as $a => $b) {
					if ($a == "svpname") {
						$svpname = $b;
						break;
					}
				}
				if (isset ($svpname))
					$res = $mdb2->rollback ($svpname);
				else
					$res = $mdb2->rollback ();
				handle_pear_error ($res, $reply);
				$status = "OK";
				break;
			case 'META':
				if (!isset ($_SESSION['key']))
					throw new GdaException('Protocol error', true);
				if ($token == "-")
					throw new GdaException('Not authenticated', true);

				$args = array ();
				foreach ($child->children() as $sqlnode) {
					if ($sqlnode->getName() == "arg") {
						foreach ($sqlnode->attributes() as $a => $b) {
							if ($a == "name") {
								$args[$b] = $sqlnode[0];
								break;
							}
						}
					}
				}
				foreach ($child->attributes() as $a => $b) {
					if ($a == "type") {
						$type = $b;
						break;
					}
				}
				if (! isset ($type))
					throw new GdaException('Bad XML input', true);

				if (!isset ($mdb2))
					$mdb2 = real_connect ($_SESSION['dsn'], $reply);

				do_meta ($reply, $mdb2, $type, $args);
				$status = "OK";
				break;
			default:
				throw new GdaException('Unknown command '.$child[0], false);
			}
		}
		else {
			throw new GdaException('Bad XML input', true);
		}
	}
		
	if ($status != "CLOSED") {
		$chal = rand_str ();
		$_SESSION['challenge'] = $chal;
		$reply->addChild ('challenge', $chal);
	}

	if ($status != "")
		$reply->addChild ('status', $status);
	$reply->addChild ("counter", $_SESSION ["counter"]);
	if (isset ($debug) && $debug && $status != "CLOSED") {
		$reply->addChild ('session', htmlspecialchars(SID));
	}
		
	/* output reply */
	$retval = gda_add_hash ($_SESSION['key'], $reply->asXml());
	if ($apply_md5_to_key) {
		/* modify $key so it's possible for the client to forget about password */
		$_SESSION['key'] = md5 ($_SESSION['key']);
	}
	return $retval;
}

function do_prepare ($reply, &$mdb2, $sql, $type_is_result, $argsnode)
{
	/* prepare argument types */
	if ($argsnode) {
		$argtypes = array();
		foreach ($argsnode->children() as $node) {
			if ($node->getName() == "arg") {
				foreach($node->attributes() as $a => $b) {
					if ($a == "type") {
						$argtypes[] = $b;
						break;
					}
				}
			}
		}
	}
	else
		$argtypes = null;
	if ($type_is_result)
		$res = $mdb2->prepare((string) $sql, $argtypes, MDB2_PREPARE_RESULT);
	else
		$res = $mdb2->prepare((string) $sql, $argtypes, MDB2_PREPARE_MANIP);
	handle_pear_error ($res, $reply);
	
	global $prepared_statements;
	$pstmt_hash = md5 ((string) $sql);
	$prepared_statements [$pstmt_hash] = $res;
	$reply->addChild ('preparehash', $pstmt_hash);
	return $res;
}

function do_unprepare ($reply, &$mdb2, $pstmt_hash)
{
	/* get prepared statement */
	global $prepared_statements;
	if (isset ($pstmt_hash))
		$prep = $prepared_statements [(string) $pstmt_hash];
	if (isset ($prep)) {
		$prep->free();
		unset ($prepared_statements [(string) $pstmt_hash]);
	}
}

function do_exec ($reply, &$mdb2, $pstmt_hash, $sql, $type_is_result, $argsnode)
{
	/* get prepared statement */
	global $prepared_statements;
	if (isset ($pstmt_hash))
		$prep = $prepared_statements [(string) $pstmt_hash];

	if (!isset ($prep))
		$prep = do_prepare ($reply, $mdb2, (string) $sql, $type_is_result, $argsnode);
	
	/* handle arguments */
	if ($argsnode) {
		$args = array();
		foreach ($argsnode->children() as $node) {
			if ($node->getName() == "arg") {
				$args[] = $node[0];
			}
		}
	}
	else
		$args = null;

	/* actual execution */
	$res = $prep->execute ($args);
	handle_pear_error ($res, $reply);

	if ($type_is_result) {
		$node = $reply->addChild ("gda_array", null);
		$ncols = $res->numCols();

		/* compute field type */
		$tableinfo = $mdb2->reverse->tableInfo ($res, NULL);
		if (! PEAR::isError ($tableinfo)) {
			//var_dump($tableinfo);
			$gtypes = array();
			$dbtypes = array();
			for ($i = 0; $i < $ncols; $i++) {
				$gtypes [$i] = mdb2_type_to_gtype ($tableinfo[$i]['mdb2type']);
				$dbtypes [$i] = $tableinfo[$i]['type'];
			}
		}

		$colnames = $res->getColumnNames ();
		if (PEAR::isError ($colnames))
			unset ($colnames);
		for ($i = 0; $i < $ncols; $i++) {
			$field = $node->addChild ("gda_array_field", null);
			$field->addAttribute ("id", "FI".$i);
			if (isset ($colnames)) {
				foreach ($colnames as $name => $pos) {
					if ($pos == $i) {
						$field->addAttribute ("name", $name);
						break;
					}
				}
			}
			if (isset ($gtypes))
				$field->addAttribute ("gdatype", $gtypes[$i]);
			else
				$field->addAttribute ("gdatype", "string");
			if (isset ($dbtypes) && $dbtypes[$i])
				$field->addAttribute ("dbtype", $dbtypes[$i]);
			$field->addAttribute ("nullok", "TRUE");
		}
		
		$data = $node->addChild ("gda_array_data", null);
		while (($row = $res->fetchRow())) {
			// MDB2's default fetchmode is MDB2_FETCHMODE_ORDERED
			$xmlrow = $data->addChild ("gda_array_row", null);
			for ($i = 0; $i < $ncols; $i++) {
				$val = $xmlrow->addChild ("gda_value");
				$val[0] = str_replace ("&", "&amp;", $row[$i]);
			}
		}
	}
	else {
		$reply->addChild ("impacted_rows", (string) $res);
	}
}

function do_meta ($reply, &$mdb2, $type, $args)
{
	switch ($type) {
	case "info":
		do_meta_info ($reply, &$mdb2);
		break;
	case "btypes":
		do_meta_btypes ($reply, &$mdb2);
		break;
	case "schemas":
		do_meta_schemas ($reply, &$mdb2, $args["schema_name"]);
		break;
	case "tables":
		do_meta_tables ($reply, &$mdb2, $args["table_schema"], $args["table_name"]);
		break;
	case "views":
		do_meta_views ($reply, &$mdb2, $args["table_schema"], $args["table_name"]);
		break;
	case "columns":
		do_meta_columns ($reply, &$mdb2, $args["table_schema"], $args["table_name"]);
		break;
	case "constraints_tab":
		do_meta_constraints_tab ($reply, &$mdb2,
					 $args["table_schema"], $args["table_name"], $args["constraint_name"]);
		break;
	case "constraints_ref":
		do_meta_constraints_ref ($reply, &$mdb2,
					 $args["table_schema"], $args["table_name"], $args["constraint_name"]);
		break;
	case "key_columns":
		do_meta_key_columns ($reply, &$mdb2,
				     $args["table_schema"], $args["table_name"], $args["constraint_name"]);
		break;
	case "check_columns":
		do_meta_check_columns ($reply, &$mdb2,
				       $args["table_schema"], $args["table_name"], $args["constraint_name"]);
		break;
	case "triggers":
		do_meta_triggers ($reply, &$mdb2, $args["table_schema"], $args["table_name"]);
		break;
	default:
		throw new GdaException('Unknown META command '.$type, false);
	}
}

?>
