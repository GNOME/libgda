<?php
include_once "gda-utils.php";
include_once "gda-config.php";

/*
 * REM: no session is started in this script because sessions would serialize the
 * execution of the gda-front.php and gda-worker.php scripts. Only the session ID, transmitted
 * in the URL is used here.
 */

header ('Content-type: text/plain; charset=UTF-8');

$datafile = null;

try {
	global $datafile;
	$text = $GLOBALS["HTTP_RAW_POST_DATA"];
	if ($text == "")
		throw new Exception ("Empty command");

	/* Send command */
	$cmdfile = get_command_filename ($_GET['PHPSESSID']);
	if (! file_exists ($cmdfile))
		throw new Exception ("Bad setup $cmdfile");
	
	$datafile = tempnam (session_save_path (), "GDACommand");
	//echo "DATAFILE [$datafile]\n"; flush (); ob_flush();
	if (file_put_contents ($datafile, $text) == false)
		throw new Exception ("Can't create command file ".$cmdfile);
	
	if (isset ($log))
		$log->lwrite ("COMMAND: [$text]");

	$file = fopen ($cmdfile, "w");
	if (fwrite ($file, $datafile) == false) // block until there is a reader
		throw new Exception ("Can't send command");
	fclose ($file);

	/* wait for reply, and destroy the reading file */
	$text = "";
	$replyfile = get_reply_filename ($_GET['PHPSESSID']);
	if (! file_exists ($replyfile))
		throw new Exception ("Bad setup");
	$file = fopen ($replyfile, "r");
	$datafile = fread ($file, 8192);
	fclose ($file);
	//echo "DATAFILE [$datafile]\n"; flush (); ob_flush();

	if (filesize ($datafile) == 0)
		throw new Exception ("No reply");

	$file = fopen ($datafile, 'rb');
	fpassthru ($file);
	if (isset ($log)) {
		$tmp = file_get_contents ($datafile);
		$log->lwrite ("RESPONSE: [$tmp]");
	}

	@unlink ($datafile);

	echo $text;
}
catch (Exception $e) {
	$reply = new SimpleXMLElement("<reply></reply>");
	$node = $reply->addChild ('status', "ERROR");
	$node->addAttribute ("error", $e->getMessage());

	global $init_shared;
	echo gda_add_hash ($init_shared, $reply->asXml());

	global $datafile;
	if (isset ($datafile))
		@unlink ($datafile);
}

?>
