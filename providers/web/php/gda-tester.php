<?php
session_cache_limiter('nocache');
header('Content-type: text/html; charset=UTF-8');
header('Cache-Control: public, no-cache');

include_once "gda-utils.php";
include_once "gda-exception.php";
include_once "gda-config.php";

$test_connections = false; // set to true to enable each connection testing

echo "<h1>Gda connections tester</h1>\n\n";

if (! try_include ("MDB2.php", true)) {
	echo "<b>ERROR:</b> The PEAR MDB2 extension is required\n";
	echo "<p>If you want to use the implementation provided in Libgda, then add a directive as following to the <tt>gda-config.php</tt> file:</p>";
	echo "<p><tt>set_include_path (get_include_path().\":PEAR_MDB2\");</tt></p>";
	echo "<p>Note that you still need to install the PEAR package (named 'php-pear' on a Debian system).</p>";
	exit (1);
}

if (! extension_loaded ("SimpleXML")) {
	echo "<b>ERROR</b>: The SimpleXML extension is required\n";
	echo "<p>You need to configure PHP to include this extension.</p>";
	exit (1);
}

function handle_pear_error ($res)
{
	if (PEAR::isError($res)) {
		$cause = "<p><b>Standard Message:</b> ".$res->getMessage()."</p>\n". 
			"<p><b>User Information:</b> ".$res->getUserInfo()."</p>\n".
			"<p><b>Debug Information:</b> ".$res->getDebugInfo()."</p>";
		throw new GdaException($cause, false);
	}
}

echo "\n";

if (! isset ($cnc, $dsn)) {
	echo "<p><b>ERROR:</b> No connection defined!</p>\n";
	exit (1);
}

if ($test_connections) {
	foreach ($cnc as $dbname => $dbpass) {
		echo "<h2>Connection '".$dbname."'</h2>";
		try {
			$mdb2 = MDB2::connect($dsn[$dbname]);
			handle_pear_error ($mdb2);
			echo "OK.\n";
		}
		catch (GdaException $e) {
			echo "FAILED!\n".$e->getMessage()."\n";
		}
	}
}
else {
	echo "<p>Connections listed below but not tested (set <tt>\$test_connections</tt> to <tt>true</tt> in the <tt>gda-tester.php</tt> file to change):</p>\n";
	echo "<ul>\n";
	foreach ($cnc as $dbname => $dbpass) {
		echo "<li>Connection '".$dbname."'</li>\n";
	}
	echo "</ul>\n";
}
?>
