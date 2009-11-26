<?php
session_cache_limiter('nocache');
header('Content-type: text/plain; charset=UTF-8');

include_once "gda-utils.php";
include_once "gda-exception.php";
include_once "gda-config.php";

$test_connections = false; // set to true to enable each connection testing

echo "Gda connections tester\n----------------------\n\n";

if (! try_include ("MDB2.php", true)) {
	echo "ERROR: The PEAR MDB2 extension is required\n";
	exit (1);
}

if (! extension_loaded ("SimpleXML")) {
	echo "ERROR: The SimpleXML extension is required\n";
	exit (1);
}

function handle_pear_error ($res)
{
	if (PEAR::isError($res)) {
		$cause = "\tStandard Message [".$res->getMessage()."]\n". 
			"\tUser Information [".$res->getUserInfo()."]\n".
			"\tDebug Information [".$res->getDebugInfo()."]";
		throw new GdaException($cause, false);
	}
}

echo "\n";
if ($test_connections) {
	foreach ($cnc as $dbname => $dbpass) {
		echo "Connection ".$dbname;
		try {
			$mdb2 = MDB2::connect($dsn[$dbname]);
			handle_pear_error ($mdb2);
			echo " ==> OK\n";
		}
		catch (GdaException $e) {
			echo " ==> FAILED:\n".$e->getMessage()."\n";
		}
	}
}
else {
	echo "Connections are not tested, set \$test_connections to true to enable connection testing\n";
}
?>
