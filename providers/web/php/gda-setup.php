<?php
/* Configure session cache */
session_cache_limiter('nocache');
session_start();

header('Content-type: text/plain; charset=UTF-8');

include_once "gda-utils.php";
include_once "gda-config.php";

if (! try_include ("MDB2.php") && ! try_include ("PEAR".DIRECTORY_SEPARATOR."MDB2.php")) {
	$reply = new SimpleXMLElement("<reply></reply>");
	$node = $reply->addChild ("status", "ERROR");
	$node->addAttribute ("error", "The PEAR MDB2 extension is required");
	echo gda_add_hash ($init_shared, $reply->asXml());
	session_destroy ();
	exit (1);
}

if (! extension_loaded ("SimpleXML")) {
	$reply = new SimpleXMLElement("<reply></reply>");
	$node = $reply->addChild ("status", "ERROR");
	$node->addAttribute ("error", "The SimpleXML extension is required");
	echo gda_add_hash ($init_shared, $reply->asXml());
	session_destroy ();
	exit (1);
}

$cmdfile = get_command_filename (session_id ());
$replyfile = get_reply_filename (session_id ());

umask(0);
$mode = 0600;
posix_mkfifo ($cmdfile, $mode);
posix_mkfifo ($replyfile, $mode);

if (!file_exists ($cmdfile) ||
    !file_exists ($replyfile)) {
	$reply = new SimpleXMLElement("<reply></reply>");
	$node = $reply->addChild ("status", "ERROR");
	$node->addAttribute ("error", "Can't create named pipes");
	echo gda_add_hash ($init_shared, $reply->asXml());
	
	@unlink ($cmdfile);
	@unlink ($replyfile);
	session_destroy ();
	exit (1);
}

/* all setup */
$reply = new SimpleXMLElement("<reply></reply>");
$reply->addChild ('session', htmlspecialchars(SID));
$reply->addChild ("status", "OK");
echo gda_add_hash ($init_shared, $reply->asXml());
flush ();
ob_flush ();
?>
